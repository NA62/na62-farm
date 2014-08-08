//============================================================================
// Name        : NA62 online trigger PC farm framework
// Author      : Jonas Kunze (kunze.jonas@gmail.com)
//============================================================================

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <eventBuilding/SourceIDManager.h>
#include <tbb/task.h>

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

#include "eventBuilding/L1Builder.h"
#include "eventBuilding/L2Builder.h"
#include "eventBuilding/StorageHandler.h"
#include "monitoring/MonitorConnector.h"
#include "options/MyOptions.h"
#include "socket/PacketHandler.h"
#include "socket/ZMQHandler.h"
#include "eventBuilding/EventPool.h"
#include "socket/HandleFrameTask.h"

using namespace std;
using namespace na62;

std::vector<PacketHandler*> packetHandlers;

void handle_stop(const boost::system::error_code& error, int signal_number) {
	std::cout << "Received signal " << signal_number << " - Shutting down"
			<< std::endl;
	if (!error) {
		ZMQHandler::Stop();
		AExecutable::InterruptAll();
		AExecutable::JoinAll();

		for (auto& handler : packetHandlers) {
			handler->stopRunning();
		}

		StorageHandler::OnShutDown();

		ZMQHandler::Shutdown();

		std::cout << "Cleanly shut down na62-farm" << std::endl;
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
	 * Initialize NIC handler and start gratuitous ARP request sending thread
	 */
	NetworkHandler NetworkHandler(Options::GetString(OPTION_ETH_DEVICE_NAME));
	NetworkHandler.startThread("ArpSender");

	SourceIDManager::Initialize(Options::GetInt(OPTION_TS_SOURCEID),
			Options::GetIntPairList(OPTION_DATA_SOURCE_IDS),
			Options::GetIntPairList(OPTION_CREAM_CRATES),
			Options::GetIntPairList(OPTION_INACTIVE_CREAM_CRATES));

	PacketHandler::Initialize();

	HandleFrameTask::Initialize();

	StorageHandler::Initialize();

	L1TriggerProcessor::Initialize(Options::GetInt(OPTION_L1_DOWNSCALE_FACTOR));

	L2TriggerProcessor::Initialize(Options::GetInt(OPTION_L2_DOWNSCALE_FACTOR));

	L1Builder::Initialize();
	L2Builder::Initialize();

	EventPool::Initialize();

	cream::L1DistributionHandler::Initialize(
			Options::GetInt(OPTION_MAX_TRIGGERS_PER_L1MRP),
			Options::GetInt(OPTION_NUMBER_OF_EBS),
			Options::GetInt(OPTION_MIN_USEC_BETWEEN_L1_REQUESTS),
			Options::GetString(OPTION_CREAM_MULTICAST_GROUP),
			Options::GetInt(OPTION_CREAM_RECEIVER_PORT),
			Options::GetInt(OPTION_CREAM_MULTICAST_PORT));

	/*
	 * Monitor
	 */
	LOG(INFO)<<"Starting Monitoring Services";
	monitoring::MonitorConnector monitor;
	IPCHandler::updateState(RUNNING);
	monitor.startThread("MonitorConnector");

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
		tbb::task::enqueue(*handler, tbb::priority_t::priority_high);
	}

	/*
	 * Join PacketHandler and other threads
	 */
//	dummy->wait_for_all();
//	dummy->destroy(*dummy);
	AExecutable::JoinAll();
	return 0;
}
