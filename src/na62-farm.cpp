//============================================================================
// Name        : NA62 online trigger PC farm framework
// Author      : Jonas Kunze (kunze.jonas@gmail.com)
//============================================================================

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <eventBuilding/SourceIDManager.h>
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

#include "eventBuilding/EventBuilder.h"
#include "eventBuilding/StorageHandler.h"
#include "monitoring/MonitorConnector.h"
#include "options/MyOptions.h"
#include "socket/PacketHandler.h"
#include "socket/ZMQHandler.h"

using namespace std;
using namespace na62;

std::vector<PacketHandler*> packetHandlers;
std::vector<EventBuilder*> eventBuilders;

void handle_stop(const boost::system::error_code& error, int signal_number) {
	if (!error) {
		std::cout << "Received signal " << signal_number << "! Shutting down"
				<< std::endl;

		AExecutable::InterruptAll();
		AExecutable::JoinAll();

		for (auto eventBuilder : eventBuilders) {
			delete eventBuilder;
		}

		for (auto packetHandler : packetHandlers) {
			delete packetHandler;
		}

		StorageHandler::OnShutDown();

		ZMQHandler::Destroy();

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
			Options::GetIntPairList(OPTION_CREAM_CRATES));

	PacketHandler::Initialize();

	StorageHandler::Initialize();

	EventBuilder::Initialize();

	L1TriggerProcessor::Initialize(Options::GetInt(OPTION_L1_DOWNSCALE_FACTOR));

	L2TriggerProcessor::Initialize(Options::GetInt(OPTION_L2_DOWNSCALE_FACTOR));

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
	 * Packet Handler
	 */
	unsigned int numberOfPacketHandler = PFringHandler::GetNumberOfQueues();
	std::cout << "Starting " << numberOfPacketHandler
			<< " PacketHandler threads" << std::endl;

	for (unsigned int i = 0; i < numberOfPacketHandler; i++) {
		packetHandlers.push_back(new PacketHandler());
		LOG(INFO)<< "Binding PacketHandler " << i << " to core " << i << "!";
		packetHandlers[i]->startThread(i, "PacketHandler" + std::to_string(i),
				i, 15);
	}

	/*
	 * Event Builder
	 */
	unsigned int numberOfEB = Options::GetInt(OPTION_NUMBER_OF_EBS);
	LOG(INFO)<< "Starting " << numberOfEB << " EventBuilder threads"
	<< std::endl;

	for (unsigned int i = 0; i < numberOfEB; i++) {
		eventBuilders.push_back(new EventBuilder());
		eventBuilders[i]->startThread(i, "EventBuilder" + std::to_string(i), -1,
				15);
	}

	/*
	 * L1 Distribution handler
	 */
	cream::L1DistributionHandler l1Handler;
	l1Handler.startThread("L1DistributionHandler");

	AExecutable::JoinAll();
	return 0;
}
