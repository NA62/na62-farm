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
#include <socket/PFringHandler.h>
#include <unistd.h>
#include <csignal>
#include <iostream>
#include <vector>
#include <l1/L1TriggerProcessor.h>
#include <l2/L2TriggerProcessor.h>

#include "eventBuilding/BuildL1Task.h"
#include "eventBuilding/BuildL2Task.h"
#include "eventBuilding/StorageHandler.h"
#include "monitoring/MonitorConnector.h"
#include "options/MyOptions.h"
#include "socket/PacketHandler.h"
#include "socket/ZMQHandler.h"
#include "eventBuilding/EventPool.h"

using namespace std;
using namespace na62;

std::vector<PacketHandler*> packetHandlers;

void handle_stop(const boost::system::error_code& error, int signal_number) {
	if (!error) {
		ZMQHandler::Stop();
		AExecutable::InterruptAll();
		AExecutable::JoinAll();

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

	PFringHandler pfRingHandler("dna0");

	SourceIDManager::Initialize(Options::GetInt(OPTION_TS_SOURCEID),
			Options::GetIntPairList(OPTION_DATA_SOURCE_IDS),
			Options::GetIntPairList(OPTION_CREAM_CRATES),
			Options::GetIntPairList(OPTION_INACTIVE_CREAM_CRATES));

	PacketHandler::Initialize();

	StorageHandler::Initialize();

	L1TriggerProcessor::Initialize(Options::GetInt(OPTION_L1_DOWNSCALE_FACTOR));

	L2TriggerProcessor::Initialize(Options::GetInt(OPTION_L2_DOWNSCALE_FACTOR));

	BuildL1Task::Initialize();
	BuildL2Task::Initialize();

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
	unsigned int numberOfPacketHandler = PFringHandler::GetNumberOfQueues();
	std::cout << "Starting " << numberOfPacketHandler
			<< " PacketHandler threads" << std::endl;

	tbb::task* dummy = new (tbb::task::allocate_root()) tbb::empty_task;
	dummy->set_ref_count(numberOfPacketHandler + 1);

	for (unsigned int i = 0; i < numberOfPacketHandler; i++) {
		PacketHandler* handler = new (tbb::task::allocate_root()) PacketHandler(
				i);
		packetHandlers.push_back(handler);
		dummy->spawn(*handler);
	}

	/*
	 * Join PacketHandler and other threads
	 */
	dummy->wait_for_all();
	dummy->destroy(*dummy);
	AExecutable::JoinAll();
	return 0;
}
