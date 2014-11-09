//============================================================================
// Name        : NA62 online trigger PC farm framework
// Author      : Jonas Kunze (kunze.jonas@gmail.com)
//============================================================================

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <eventBuilding/SourceIDManager.h>
#include <tbb/task.h>
#include <thread>

#ifdef USE_GLOG
#include <glog/logging.h>
#endif
#include <LKr/L1DistributionHandler.h>
#include <monitoring/IPCHandler.h>
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

#include "eventBuilding/L1Builder.h"
#include "eventBuilding/L2Builder.h"
#include "eventBuilding/StorageHandler.h"
#include "monitoring/MonitorConnector.h"
#include "options/MyOptions.h"
#include "socket/PacketHandler.h"
#include "socket/ZMQHandler.h"
#include "socket/HandleFrameTask.h"
#include "monitoring/CommandConnector.h"
#include "straws/StrawReceiver.h"

using namespace std;
using namespace na62;

std::vector<PacketHandler*> packetHandlers;

void handle_stop(const boost::system::error_code& error, int signal_number) {
	google::ShutdownGoogleLogging();
	std::cout << "#############################################" << std::endl;
	std::cout << "#############################################" << std::endl;
	std::cout << "#############################################" << std::endl;
	std::cout << "#############################################" << std::endl;
	std::cout << "#############################################" << std::endl;
	std::cout << "Received signal " << signal_number << " - Shutting down"
			<< std::endl;

	IPCHandler::updateState(INITIALIZING);
	usleep(100);
	if (!error) {
		ZMQHandler::Stop();
		AExecutable::InterruptAll();

		LOG(INFO)<< "Stopping packet handlers";
		for (auto& handler : packetHandlers) {
			handler->stopRunning();
		}

		LOG(INFO)<< "Stopping storage handler";
		StorageHandler::onShutDown();

		LOG(INFO)<< "Stopping STRAW receiver";
		StrawReceiver::onShutDown();

		usleep(1000);
		LOG(INFO)<< "Stopping IPC handler";
		IPCHandler::shutDown();

		LOG(INFO)<< "Stopping ZMQ handler";
		ZMQHandler::shutdown();

		LOG(INFO)<< "Cleanly shut down na62-farm";
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

	/*
	 * Static Class initializations
	 */
	MyOptions::Load(argc, argv);

	ZMQHandler::Initialize(Options::GetInt(OPTION_ZMQ_IO_THREADS));

	/*
	 * initialize NIC handler and start gratuitous ARP request sending thread
	 */
	NetworkHandler NetworkHandler(Options::GetString(OPTION_ETH_DEVICE_NAME));
	NetworkHandler.startThread("ArpSender");

	SourceIDManager::Initialize(Options::GetInt(OPTION_TS_SOURCEID),
			Options::GetIntPairList(OPTION_DATA_SOURCE_IDS),
			Options::GetIntPairList(OPTION_CREAM_CRATES),
			Options::GetIntPairList(OPTION_INACTIVE_CREAM_CRATES),
			Options::GetInt(OPTION_MUV_CREAM_CRATE_ID));

	/*
	 * Monitor
	 */
	LOG(INFO)<<"Starting Monitoring Services";
	monitoring::MonitorConnector monitor;
	monitoring::MonitorConnector::setState(INITIALIZING);
	monitor.startThread("MonitorConnector");

	PacketHandler::initialize();

	HandleFrameTask::initialize();

	StorageHandler::initialize();
	StrawReceiver::initialize();

	L1Builder::initialize();
	L2Builder::initialize();

	Event::initialize();

	Event::setPrintMissingSourceIds(
			MyOptions::GetBool(OPTION_PRINT_MISSING_SOURCES));

	EventPool::Initialize(Options::GetInt(
	OPTION_MAX_NUMBER_OF_EVENTS_PER_BURST));

	cream::L1DistributionHandler::Initialize(
			Options::GetInt(OPTION_MAX_TRIGGERS_PER_L1MRP),
			Options::GetInt(OPTION_NUMBER_OF_EBS),
			Options::GetInt(OPTION_MIN_USEC_BETWEEN_L1_REQUESTS),
			Options::GetStringList(OPTION_CREAM_MULTICAST_GROUP),
			Options::GetInt(OPTION_CREAM_RECEIVER_PORT),
			Options::GetInt(OPTION_CREAM_MULTICAST_PORT));

	/*
	 * L1 Distribution handler
	 */
	cream::L1DistributionHandler l1Handler;
	l1Handler.startThread("L1DistributionHandler");

	/*
	 * Packet Handler
	 */
	unsigned int numberOfPacketHandler = NetworkHandler::GetNumberOfQueues();
	std::cout << "Starting " << numberOfPacketHandler
			<< " PacketHandler threads" << std::endl;

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
