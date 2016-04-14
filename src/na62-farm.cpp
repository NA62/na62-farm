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
#include <monitoring/FarmStatistics.h>
#include <options/Options.h>
#include <socket/NetworkHandler.h>
#include <unistd.h>
#include <csignal>
#include <iostream>
#include <vector>
#include <l1/L1TriggerProcessor.h>
#include <l2/L2TriggerProcessor.h>
#include <eventBuilding/EventPool.h>
#include <eventBuilding/Event.h>
#include <l1/L1DistributionHandler.h>
#include <options/TriggerOptions.h>
#include <storage/EventSerializer.h>

#include "eventBuilding/L1Builder.h"
#include "eventBuilding/L2Builder.h"
#include "eventBuilding/StorageHandler.h"
#include "monitoring/MonitorConnector.h"
#include "options/MyOptions.h"
#include "socket/PacketHandler.h"
#include "socket/TaskProcessor.h"
#include "socket/ZMQHandler.h"
#include "socket/HandleFrameTask.h"
#include "monitoring/CommandConnector.h"
//#include "straws/StrawReceiver.h"


using namespace std;
using namespace na62;

std::vector<PacketHandler*> packetHandlers;
std::vector<TaskProcessor*> taskProcessors;

void handle_stop(const boost::system::error_code& error, int signal_number) {
#ifdef USE_GLOG
	google::ShutdownGoogleLogging();
#endif
	LOG_INFO<< "#############################################" << ENDL;
	LOG_INFO<< "#############################################" << ENDL;
	LOG_INFO<< "#############################################" << ENDL;
	LOG_INFO<< "Received signal " << signal_number << " - Shutting down"
	<< ENDL;

	IPCHandler::updateState(INITIALIZING);
	usleep(100);
	if (!error) {
		ZMQHandler::Stop();
		AExecutable::InterruptAll();
		FarmStatistics::stopRunning();

		LOG_INFO<< "Stopping packet handlers";
		for (auto& handler : packetHandlers) {
			handler->stopRunning();
		}

		LOG_INFO<< "Stopping storage handler";
		StorageHandler::onShutDown();

		//LOG_INFO<< "Stopping STRAW receiver";
		//StrawReceiver::onShutDown();

		usleep(1000);
		LOG_INFO<< "Stopping IPC handler";
		IPCHandler::shutDown();

		LOG_INFO<< "Stopping ZMQ handler";
		ZMQHandler::shutdown();

		LOG_INFO<< "Stopping Burst handler";
		BurstIdHandler::shutDown();

		LOG_INFO<< "Cleanly shut down na62-farm";
		exit(0);
	}
}

void onBurstFinished() {
    static std::atomic<uint> incompleteEvents_;
    incompleteEvents_ = 0;


#ifdef HAVE_TCMALLOC
    // Do it with parallel_for using tbb if tcmalloc is linked
	tbb::parallel_for(
			tbb::blocked_range<uint_fast32_t>(0, EventPool::getLargestTouchedEventnumberIndex() + 1,
					EventPool::getLargestTouchedEventnumberIndex() / std::thread::hardware_concurrency()),
					[](const tbb::blocked_range<uint_fast32_t>& r) {
		for(size_t index=r.begin();index!=r.end(); index++) {
			Event* event = EventPool::getEventByIndex(index);
			if(event == nullptr) continue;
			if (event->isUnfinished()) {
					//LOG_ERROR << "Incomplete event " << (uint)(event->getEventNumber());
				if(event->isLastEventOfBurst()) {
					LOG_ERROR << "type = EOB : Handling unfinished EOB event " << event->getEventNumber()<< ENDL;
					StorageHandler::SendEvent(event);
				}
				++incompleteEvents_;
				event->updateMissingEventsStats();
				EventPool::freeEvent(event);
			}
		}
	});
#else
	for (uint idx = 0; idx != EventPool::getLargestTouchedEventnumberIndex() + 1; ++idx) {
		Event* event = EventPool::getEventByIndex(idx);
		if (event->isUnfinished()) {
			++incompleteEvents_;
			// if EOB send event to merger as in L2Builder.cpp
			EventPool::freeEvent(event);
		}
	}
#endif

	if(incompleteEvents_ > 0) {
		LOG_ERROR << "type = EOB : Dropped " << incompleteEvents_ << " events in burst ID = " << (int) BurstIdHandler::getCurrentBurstId() << ".";
	}
}

int main(int argc, char* argv[]) {
	/*
	 * Signals
	 */
	boost::asio::io_service signalService;
	boost::asio::signal_set signals(signalService, SIGINT, SIGTERM, SIGQUIT);
	signals.async_wait(handle_stop);
	boost::thread signalThread(
			boost::bind(&boost::asio::io_service::run, &signalService));


	L1TriggerProcessor::registerDownscalingAlgorithms();

	L1TriggerProcessor::registerReductionAlgorithms();
	/*
	 * Static Class initializations
	 */
	TriggerOptions::Load(argc, argv);
	MyOptions::Load(argc, argv);

	ZMQHandler::Initialize(Options::GetInt(OPTION_ZMQ_IO_THREADS));

	L1TriggerProcessor::initialize(
			TriggerOptions::GetDouble(OPTION_L1_BYPASS_PROBABILITY));
	L2TriggerProcessor::initialize(
			TriggerOptions::GetDouble(OPTION_L2_BYPASS_PROBABILITY));


	FarmStatistics::init();
	FarmStatistics farmstats;
	farmstats.startThread("StatisticsWriter");
	/*
	 * initialize NIC handler and start gratuitous ARP request sending thread
	 */
	NetworkHandler NetworkHandler(Options::GetString(OPTION_ETH_DEVICE_NAME));
	NetworkHandler.startThread("ArpSender");

	SourceIDManager::Initialize(Options::GetInt(OPTION_TS_SOURCEID),
			Options::GetIntPairList(OPTION_DATA_SOURCE_IDS),
			Options::GetIntPairList(OPTION_L1_DATA_SOURCE_IDS));

	BurstIdHandler::initialize(Options::GetInt(OPTION_FIRST_BURST_ID), &onBurstFinished);

	HandleFrameTask::initialize();

	EventSerializer::initialize();
	StorageHandler::initialize();
	//StrawReceiver::initialize();

	L1Builder::initialize();
	L2Builder::initialize();

	Event::initialize(MyOptions::GetBool(OPTION_PRINT_MISSING_SOURCES));

    // Get the list of farm nodes and find my position
    vector<std::string> nodes = Options::GetStringList(OPTION_FARM_HOST_NAMES);
    std::string myIP = EthernetUtils::ipToString(EthernetUtils::GetIPOfInterface(Options::GetString(OPTION_ETH_DEVICE_NAME)));
    uint logicalNodeID = 0xffffffff;
    for (size_t i=0; i< nodes.size(); ++i) {
            if (myIP==nodes[i]) {
                    logicalNodeID = i;
                    break;
            }
    }
    if (logicalNodeID == 0xffffffff) {
            LOG_ERROR << "You must provide a list of farm nodes IP addresses containing the IP address of this node!";
            exit(1);
    }

    EventPool::initialize(Options::GetInt(OPTION_MAX_NUMBER_OF_EVENTS_PER_BURST), nodes.size(), logicalNodeID, Options::GetInt(OPTION_NUMBER_OF_FRAGS_PER_L0MEP));

	l1::L1DistributionHandler::Initialize(
			Options::GetInt(OPTION_MAX_TRIGGERS_PER_L1MRP),
			Options::GetInt(OPTION_MIN_USEC_BETWEEN_L1_REQUESTS),
			Options::GetStringList(OPTION_CREAM_MULTICAST_GROUP),
			Options::GetInt(OPTION_CREAM_RECEIVER_PORT),
			Options::GetInt(OPTION_CREAM_MULTICAST_PORT));

	/*
	 * Burst Handler
	 */
	LOG_INFO << "Start burst handler thread.";
	BurstIdHandler bHandler;
	bHandler.startThread("BurstHandler");
	/*
	 * Monitor
	 */
	LOG_INFO<<"Starting Monitoring Services";
	monitoring::MonitorConnector monitor;
	monitoring::MonitorConnector::setState(INITIALIZING);
	monitor.startThread("MonitorConnector");

	/*
	 * L1 Distribution handler
	 */
	l1::L1DistributionHandler l1Handler;
	l1Handler.startThread("L1DistributionHandler");

	/*
	 * Packet Handler
	 */
	unsigned int numberOfPacketHandler = NetworkHandler::GetNumberOfQueues();
	LOG_INFO<< "Starting " << numberOfPacketHandler
	<< " PacketHandler threads" << ENDL;

	for (unsigned int i = 0; i < numberOfPacketHandler; i++) {
		PacketHandler* handler = new PacketHandler(i);
		packetHandlers.push_back(handler);

		uint coresPerSocket = std::thread::hardware_concurrency() / 2/*hyperthreading*/;
		uint cpuMask = i % 2 == 0 ? i / 2 : coresPerSocket + i / 2;
		handler->startThread(i, "PacketHandler", cpuMask, 25,
				MyOptions::GetInt(OPTION_PH_SCHEDULER));

	}

	//for (unsigned int i = 0; i < 4; i++) {
	for (unsigned int i = 0; i < std::thread::hardware_concurrency() - numberOfPacketHandler; i++) {
		TaskProcessor* tp = new TaskProcessor();
		taskProcessors.push_back(tp);
		tp->startThread(i, "TaskProcessor");

	}

	CommandConnector c;
	c.startThread(0, "Commandconnector", -1, 1);
	monitoring::MonitorConnector::setState(RUNNING);

	/*
	 * Join PacketHandler and other threads
	 */
	AExecutable::JoinAll();
	return 0;
}
