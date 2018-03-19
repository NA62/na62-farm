//============================================================================
// Name        : NA62 online trigger PC farm framework
// Author      : Jonas Kunze (kunze.jonas@gmail.com)
//============================================================================

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <eventBuilding/SourceIDManager.h>
#include <tbb/task.h>
#include <tbb/tbb.h>
#include <thread>
#include <atomic>

#include <monitoring/IPCHandler.h>
#include <monitoring/BurstIdHandler.h>
//#include <monitoring/FarmStatistics.h>
#include <monitoring/DetectorStatistics.h>
#include <options/Options.h>
#include <socket/NetworkHandler.h>
#include <unistd.h>
#include <csignal>
#include <iostream>
#include <fstream>
#include <vector>
#include <l1/L1TriggerProcessor.h>
#include <l2/L2TriggerProcessor.h>
#include <common/HLTriggerManager.h>
#include <struct/HLTConfParams.h>
#include <eventBuilding/EventPool.h>
#include <eventBuilding/Event.h>
#include <l1/L1DistributionHandler.h>
#include <options/TriggerOptions.h>
#include <storage/SmartEventSerializer.h>

#include "eventBuilding/L1Builder.h"
#include "eventBuilding/L2Builder.h"
#include "eventBuilding/StorageHandler.h"
#include "monitoring/MonitorConnector.h"
#include "monitoring/HltStatistics.h"
#include "options/MyOptions.h"
#include "socket/PacketHandler.h"
#include "socket/TaskProcessor.h"
#include "socket/ZMQHandler.h"
#include "socket/HandleFrameTask.h"
#include "monitoring/CommandConnector.h"

#ifdef USE_SHAREDMEMORY
#include "SharedMemory/SharedMemoryManager.h"
#include "SharedMemory/QueueReceiver.h"
#include "SharedMemory/PoolParser.h"
#endif

#include "utils/Shutdown.h"

using namespace std;
using namespace na62;

std::vector<PacketHandler*> packetHandlers;
std::vector<TaskProcessor*> taskProcessors;

class FarmShutdown: public Shutdown {
private:
	void myHandleStop(const boost::system::error_code& error, int signal_number) {
#ifdef USE_GLOG
		google::ShutdownGoogleLogging();
#endif
		LOG_INFO("#############################################");
		LOG_INFO("#############################################");
		LOG_INFO("#############################################");
		LOG_INFO("Received signal " << signal_number << " - Shutting down");

		IPCHandler::updateState(INITIALIZING);
		usleep(100);
		if (!error) {
			ZMQHandler::Stop();
			AExecutable::InterruptAll();

			LOG_INFO("Stopping packet handlers");
			for (auto& handler : packetHandlers) {
				handler->stopRunning();
			}

			LOG_INFO("Stopping storage handler");
			StorageHandler::onShutDown();

			//LOG_INFO("Stopping STRAW receiver");
			//StrawReceiver::onShutDown();

			usleep(1000);
			LOG_INFO("Stopping IPC handler");
			IPCHandler::shutDown();

			LOG_INFO("Stopping ZMQ handler");
			ZMQHandler::shutdown();

			LOG_INFO("Stopping Burst handler");
			BurstIdHandler::shutDown();

			LOG_INFO("Cleanly shut down na62-farm");
			exit(0);
		}
	}
};



//void handle_stop(const boost::system::error_code& error, int signal_number) {
//#ifdef USE_GLOG
//	google::ShutdownGoogleLogging();
//#endif
//	LOG_INFO("#############################################");
//	LOG_INFO("#############################################");
//	LOG_INFO("#############################################");
//	LOG_INFO("Received signal " << signal_number << " - Shutting down");
//
//	IPCHandler::updateState(INITIALIZING);
//	usleep(100);
//	if (!error) {
//		ZMQHandler::Stop();
//		AExecutable::InterruptAll();
//
//		LOG_INFO("Stopping packet handlers");
//		for (auto& handler : packetHandlers) {
//			handler->stopRunning();
//		}
//
//		LOG_INFO("Stopping storage handler");
//		StorageHandler::onShutDown();
//
//		//LOG_INFO("Stopping STRAW receiver");
//		//StrawReceiver::onShutDown();
//
//		usleep(1000);
//		LOG_INFO("Stopping IPC handler");
//		IPCHandler::shutDown();
//
//		LOG_INFO("Stopping ZMQ handler");
//		ZMQHandler::shutdown();
//
//		LOG_INFO("Stopping Burst handler");
//		BurstIdHandler::shutDown();
//
//		LOG_INFO("Cleanly shut down na62-farm");
//		exit(0);
//	}
//}

void onBurstFinished() {
	static std::atomic<uint> incompleteEvents_;
	incompleteEvents_ = 0;


	LOG_INFO("@@@onBurstFinished SOB timestamp was: " <<BurstIdHandler::getSOBTime());
	LOG_INFO("@@@onBurstFinished Cleanup burst " << (int) BurstIdHandler::getCurrentBurstId() << " with EOB time " << (int) BurstIdHandler::getEOBTime() << ".");


#ifdef USE_SHAREDMEMORY
	if(SharedMemoryManager::getTriggerQueue()->get_num_msg() != 0) {
		LOG_ERROR( (int) SharedMemoryManager::getTriggerQueue()->get_num_msg() <<" events are still waiting to be processed by L1 at EOB!!!!");
		SharedMemoryManager::clearTriggerQueue();
		//IPCHandler::sendCommand("restart_processors");
	}
	//No event are waiting to be processed at L1
	//Check consistency of free location queue
	SharedMemoryManager::checkTriggerFreeQueueConsistency(); 

	if(SharedMemoryManager::getTriggerResponseQueue()->get_num_msg() != 0) {
			//Don't think that this can happen..
			LOG_ERROR("Some trigger results are still waiting to send L1 Request!!!!");
	}
#endif


	// Do it with parallel_for using tbb if tcmalloc is linked
	tbb::parallel_for(
			tbb::blocked_range<uint_fast32_t>(0,
					EventPool::getLargestTouchedEventnumberIndex() + 1,
					EventPool::getLargestTouchedEventnumberIndex()
							/ std::thread::hardware_concurrency()),
			[](const tbb::blocked_range<uint_fast32_t>& r) {
				for(size_t index=r.begin();index!=r.end(); index++) {
					Event* event = EventPool::getEventByIndex(index);
					if(event == nullptr) continue;
					if (event->isLastEventOfBurst()) {
						LOG_INFO("Processing last event of burst " << (int) event->getBurstID());
					}
					if (event->isUnfinished()) {
						if(event->isLastEventOfBurst()) {
							LOG_ERROR("type = EOB : Handling unfinished EOB event " << event->getEventNumber());
							StorageHandler::SendEvent(event);
						}
						++incompleteEvents_;
						event->updateMissingEventsStats();
						EventPool::freeEvent(event);
					}
				}
			});

	if (incompleteEvents_ > 0) {
		LOG_ERROR("type = EOB : Dropped " << incompleteEvents_ << " events in burst ID = " << (int) BurstIdHandler::getCurrentBurstId() << ".");
	//	LOG_ERROR (DetectorStatistics::L0RCInfo());
	//	LOG_ERROR (DetectorStatistics::L1RCInfo());

	}


	//Missing sources
	IPCHandler::sendStatistics("MonitoringL0Data", DetectorStatistics::L0RCInfo());
	IPCHandler::sendStatistics("MonitoringL1Data", DetectorStatistics::L1RCInfo());
	DetectorStatistics::clearL0DetectorStatistics();
	DetectorStatistics::clearL1DetectorStatistics();


	//Resetting time graphs
#ifdef MEASURE_TIME
	L1Builder::ResetL0BuildingTimeCumulative();
	L1Builder::ResetL0BuildingTimeMax();
	L1Builder::ResetL1ProcessingTimeCumulative();
	L1Builder::ResetL1ProcessingTimeMax();

	L2Builder::ResetL1BuildingTimeCumulative();
	L2Builder::ResetL1BuildingTimeMax();
	L2Builder::ResetL2ProcessingTimeCumulative();
	L2Builder::ResetL2ProcessingTimeMax();

	L1Builder::ResetL0BuidingTimeVsEvtNumber();
	L2Builder::ResetL1BuidingTimeVsEvtNumber();
	L1Builder::ResetL1ProcessingTimeVsEvtNumber();
	L2Builder::ResetL2ProcessingTimeVsEvtNumber();

#endif

	//Updating PerBurstCounters
	for (auto& key : HltStatistics::extractKeys()) {
		IPCHandler::sendStatistics(key + "PerBurst", std::to_string(HltStatistics::getRollingCounter(key)));
	}
	//Updating PerBurstCounters
	for (auto& key : HltStatistics::extractDimensionalKeys()) {
		IPCHandler::sendStatistics(key + "PerBurst", HltStatistics::serializeDimensionalCounter(key));
	}

	IPCHandler::sendStatistics("EOBStatsL1", HltStatistics::fillL1Eob());
	IPCHandler::sendStatistics("EOBStatsL2", HltStatistics::fillL2Eob());


	//Resetting ALL HLT statistics
	HltStatistics::resetCounters();
	HandleFrameTask::resetCounters();
	Event::resetCounters();

	//Memory monitor
	int tSize = 0, resident = 0, share = 0;
	ifstream buffer("/proc/self/statm");
	buffer >> tSize >> resident >> share;
	buffer.close();

	long page_size_kb = sysconf(_SC_PAGE_SIZE) / 1024; // in case x86-64 is configured to use 2MB pages
	double rss = resident * page_size_kb;
	double shared_mem = share * page_size_kb;
	if (rss > 30000000) {
		LOG_WARNING("type=memstat RSS - " + std::to_string(int(rss/1000)) + " MB. Shared Memory - " + std::to_string(int(shared_mem/1000)) + " MB. Private Memory - " + std::to_string(int((rss - shared_mem)/1000)) + "MB" );
		LOG_ERROR("Memory LEAK!!! Terminating process");
		exit(-1);
	}
}

int main(int argc, char* argv[]) {

//	/*
//	 * Signals
//	 */
//	boost::asio::io_service signalService;
//	boost::asio::signal_set signals(signalService, SIGINT, SIGTERM, SIGQUIT);
//	signals.async_wait(handle_stop);
//	boost::thread signalThread(
//			boost::bind(&boost::asio::io_service::run, &signalService));
	/*
	 * Signals
	 */
	LOG_INFO("Initializing safe ShutDown");
	FarmShutdown safeShutdown;
	safeShutdown.init();

	/*
	 * Static Class initializations
	 */
	TriggerOptions::Load(argc, argv);
	MyOptions::Load(argc, argv);


	try {
		ZMQHandler::Initialize(Options::GetInt(OPTION_ZMQ_IO_THREADS));
	} catch (const zmq::error_t& ex) {
		LOG_ERROR("Failed to initialize ZMQ because: " << ex.what());
		exit(1);
	}

	HLTStruct HLTConfParams;
	HLTriggerManager::fillStructFromXMLFile(HLTConfParams);
	L1TriggerProcessor::initialize(HLTConfParams.l1);
	L2TriggerProcessor::initialize(HLTConfParams.l2);

	// Get the list of farm nodes and find my position
	vector<std::string> nodes = Options::GetStringList(OPTION_FARM_HOST_NAMES);
	std::string myIP = EthernetUtils::ipToString(
			EthernetUtils::GetIPOfInterface(
					Options::GetString(OPTION_ETH_DEVICE_NAME)));
	uint logicalNodeID = 0xffffffff;
	for (size_t i = 0; i < nodes.size(); ++i) {
		if (myIP == nodes[i]) {
			logicalNodeID = i;
			break;
		}
	}
	if (logicalNodeID == 0xffffffff) {
		LOG_ERROR(
				"You must provide a list of farm nodes IP addresses containing the IP address of this node!");
		exit(1);
	}

	HltStatistics::initialize(logicalNodeID);

	/*
	 * initialize NIC handler and start gratuitous ARP request sending thread
	 */
	LOG_INFO("Initialize NIC handler: " << Options::GetString(OPTION_ETH_DEVICE_NAME));
	NetworkHandler NetworkHandler(Options::GetString(OPTION_ETH_DEVICE_NAME));
	LOG_INFO("Initialize Arp Send: " << Options::GetString(OPTION_ETH_DEVICE_NAME));
	NetworkHandler.startThread("ArpSender");

	SourceIDManager::Initialize(Options::GetInt(OPTION_TS_SOURCEID),
			Options::GetIntPairList(OPTION_DATA_SOURCE_IDS),
			Options::GetIntPairList(OPTION_L1_DATA_SOURCE_IDS));

	BurstIdHandler::initialize(Options::GetInt(OPTION_FIRST_BURST_ID),
			Options::GetInt(OPTION_CURRENT_RUN_NUMBER),
			&onBurstFinished);

	HandleFrameTask::initialize();

	SmartEventSerializer::initialize();
	try {
		StorageHandler::initialize();
	} catch(const zmq::error_t& ex) {
		LOG_ERROR("Failed to initialize StorageHandler because: " << ex.what());
		exit(1);
	}
	StorageHandler sh;
	sh.startThread("StorageHandler");

#ifdef USE_SHAREDMEMORY
	//Remove the shared memory if any
	//I will start with a clean memory ech time the main process is restarted
	//SharedMemoryManager::eraseAll();
	//Initialize the shared memory
	SharedMemoryManager::initialize();
	//Starting queue Receiver for processed L1
	QueueReceiver receiver;
	receiver.startThread("QueueReceiver");
	//PoolParser parser;
	//parser.startThread("PoolParser");
#endif


	L1Builder::initialize();
	L2Builder::initialize();

	Event::initialize(MyOptions::GetBool(OPTION_PRINT_MISSING_SOURCES));

//  Initialize Detector counts. L1=32 because 0=gtk, 1-30 Lkr, 31 MUV
//  20 should be changed with a proper dynamic definition of the number of L0 sources
	DetectorStatistics::init(20,32);
	DetectorStatistics::clearL0DetectorStatistics();
	DetectorStatistics::clearL1DetectorStatistics();

	/*
	 * Monitor
	 */
	LOG_INFO("Starting Monitoring Services");
	monitoring::MonitorConnector monitor;
	monitoring::MonitorConnector::setState(INITIALIZING);
	monitor.startThread("MonitorConnector");


	LOG_INFO("Initialize Event Pool.");
	EventPool::initialize(
			Options::GetInt(OPTION_MAX_NUMBER_OF_EVENTS_PER_BURST),
			nodes.size(), logicalNodeID,
			Options::GetInt(OPTION_NUMBER_OF_FRAGS_PER_L0MEP));

	l1::L1DistributionHandler::Initialize(
			Options::GetInt(OPTION_MAX_TRIGGERS_PER_L1MRP),
			Options::GetInt(OPTION_MIN_USEC_BETWEEN_L1_REQUESTS),
			Options::GetStringList(OPTION_CREAM_MULTICAST_GROUP),
			Options::GetInt(OPTION_CREAM_RECEIVER_PORT),
			//Options::GetInt(OPTION_CREAM_MULTICAST_PORT));
			Options::GetInt(OPTION_CREAM_MULTICAST_PORT),
			Options::GetString(OPTION_GATEWAY_MAC_ADDRESS));



	/*
	 * Burst Handler
	 */
	LOG_INFO("Start burst handler thread.");
	BurstIdHandler bHandler;
	bHandler.startThread("BurstHandler");


	/*
	 * L1 Distribution handler
	 */
	l1::L1DistributionHandler l1Handler;
	l1Handler.startThread("L1DistributionHandler");



	/*
	 * Packet Handler
	 */
	unsigned int numberOfPacketHandler = NetworkHandler::GetNumberOfQueues();
	LOG_INFO("Starting " << numberOfPacketHandler << " PacketHandler threads");

	for (unsigned int i = 0; i < numberOfPacketHandler; i++) {
		PacketHandler* handler = new PacketHandler(i);
		packetHandlers.push_back(handler);

		uint coresPerSocket = std::thread::hardware_concurrency()
				/ 2/*hyperthreading*/;
		uint cpuMask = i % 2 == 0 ? i / 2 : coresPerSocket + i / 2;
		handler->startThread(i, "PacketHandler", cpuMask, 25,
				MyOptions::GetInt(OPTION_PH_SCHEDULER));

	}

	//for (unsigned int i = 0; i < 4; i++) {
	for (unsigned int i = 0;
			i < std::thread::hardware_concurrency() - numberOfPacketHandler;
			i++) {
		LOG_INFO("Starting TaskProcessor no: " << i);
		TaskProcessor* tp = new TaskProcessor(i);
		taskProcessors.push_back(tp);
		tp->startThread(i, "TaskProcessor");

	}

	CommandConnector c;
	c.startThread(0, "Commandconnector", -1, 1);
	LOG_INFO("Set command connector to running.");
	monitoring::MonitorConnector::setState(RUNNING);

	/*
	 * Join PacketHandler and other threads
	 */
	AExecutable::JoinAll();
	return 0;
}
