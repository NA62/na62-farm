//============================================================================
// Name        : NA62 online trigger PC farm framework
// Author      : Jonas Kunze (kunze.jonas@gmail.com)
//============================================================================

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <eventBuilding/SourceIDManager.h>
#include <tbb/task.h>
#include <thread>

#include <LKr/L1DistributionHandler.h>
#include <monitoring/IPCHandler.h>
#include <monitoring/BurstIdHandler.h>
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
#include <options/TriggerOptions.h>
#include <storage/EventSerializer.h>

#include "eventBuilding/L1Builder.h"
#include "eventBuilding/L2Builder.h"
#include "eventBuilding/StorageHandler.h"
#include "monitoring/MonitorConnector.h"
#include "monitoring/FarmStatistics.h"
#include "options/MyOptions.h"
#include "socket/PacketHandler.h"
#include "socket/ZMQHandler.h"
#include "socket/HandleFrameTask.h"
#include "monitoring/CommandConnector.h"
#include "straws/StrawReceiver.h"

#include "socket/PcapDumper.h"


using namespace std;
using namespace na62;

std::vector<PacketHandler*> packetHandlers;

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

		LOG_INFO<< "Stopping STRAW receiver";
		StrawReceiver::onShutDown();

		usleep(1000);
		LOG_INFO<< "Stopping IPC handler";
		IPCHandler::shutDown();

		LOG_INFO<< "Stopping ZMQ handler";
		ZMQHandler::shutdown();

		LOG_INFO<< "Cleanly shut down na62-farm";
		exit(0);
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

	/*
	 * initialize NIC handler and start gratuitous ARP request sending thread
	 */
	NetworkHandler NetworkHandler(Options::GetString(OPTION_ETH_DEVICE_NAME));
	NetworkHandler.startThread("ArpSender");

	/*
		 * Time Statistics
		 */
		FarmStatistics::init();


	SourceIDManager::Initialize(Options::GetInt(OPTION_TS_SOURCEID),
			Options::GetIntPairList(OPTION_DATA_SOURCE_IDS),
			Options::GetIntPairList(OPTION_CREAM_CRATES),
			Options::GetIntPairList(OPTION_INACTIVE_CREAM_CRATES),
			Options::GetInt(OPTION_MUV_CREAM_CRATE_ID));
	LOG_INFO << "Initialized SIDMan" << ENDL;
	BurstIdHandler::initialize(Options::GetInt(OPTION_FIRST_BURST_ID));
	LOG_INFO << "Initialized BUIDHan" << ENDL;
	HandleFrameTask::initialize();
	LOG_INFO << "Initialized HanFramTask" << ENDL;
	EventSerializer::initialize();
	LOG_INFO << "Initialized Eventserializer" << ENDL;
	StorageHandler::initialize();
	LOG_INFO << "Initialized Storagehandler" << ENDL;
	StrawReceiver::initialize();
	LOG_INFO << "Initialized Strawreciever" << ENDL;
	L1Builder::initialize();
	LOG_INFO << "Initialized L1 Builder" << ENDL;
	L2Builder::initialize();
	LOG_INFO << "Initialized L2 Builder" << ENDL;

	Event::initialize(MyOptions::GetBool(OPTION_PRINT_MISSING_SOURCES),
			Options::GetBool(OPTION_WRITE_BROKEN_CREAM_INFO));

	EventPool::initialize(Options::GetInt(
	OPTION_MAX_NUMBER_OF_EVENTS_PER_BURST));

	cream::L1DistributionHandler::Initialize(
			Options::GetInt(OPTION_MAX_TRIGGERS_PER_L1MRP),
			Options::GetInt(OPTION_NUMBER_OF_EBS),
			Options::GetInt(OPTION_MIN_USEC_BETWEEN_L1_REQUESTS),
			Options::GetStringList(OPTION_CREAM_MULTICAST_GROUP),
			Options::GetInt(OPTION_CREAM_RECEIVER_PORT),
			Options::GetInt(OPTION_CREAM_MULTICAST_PORT));

	/*
	 * Monitor
	 */
	LOG_INFO<<"Starting Monitoring Services"<<ENDL;
	monitoring::MonitorConnector monitor;
	monitoring::MonitorConnector::setState(INITIALIZING);
	monitor.startThread("MonitorConnector");

	/*
	 * L1 Distribution handler
	 */
	cream::L1DistributionHandler l1Handler;
	l1Handler.startThread("L1DistributionHandler");


	/*
	 * Packet Dump
	 */
	if(Options::GetBool(OPTION_DUMP_PACKETS)){
		PcapDumper::startDump(Options::GetString(OPTION_DUMP_PACKETS_PATH));
	}



	/*
	 * Packet Handler
	 */
	unsigned int numberOfPacketHandler = NetworkHandler::GetNumberOfQueues();
	LOG_INFO<< "Starting " << numberOfPacketHandler
	<< " PacketHandler threads" << ENDL;

	for (unsigned int i = 0; i < numberOfPacketHandler; i++) {

		PacketHandler* handler = new (tbb::task::allocate_root()) PacketHandler(
				i);
		packetHandlers.push_back(handler);

		uint coresPerSocket = std::thread::hardware_concurrency()
				/ 2/*hyperthreading*/;
		uint cpuMask = i % 2 == 0 ? i / 2 : coresPerSocket + i / 2;
		handler->startThread(i, "PacketHandler", cpuMask, 15,
				MyOptions::GetInt(OPTION_PH_SCHEDULER));
	}

	CommandConnector c;
	c.startThread(0, "Commandconnector", -1, 0);
	monitoring::MonitorConnector::setState(RUNNING);

	/*
	 * Join PacketHandler and other threads
	 */
	AExecutable::JoinAll();
	return 0;
}
