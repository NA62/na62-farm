//============================================================================
// Name        : NA62 online trigger PC farm framework
// Author      : Jonas Kunze (kunze.jonas@gmail.com)
//============================================================================

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <eventBuilding/EventBuilder.h>
#include <eventBuilding/SourceIDManager.h>
#include <eventBuilding/StorageHandler.h>
#include <glog/logging.h>
#include <LKr/L1DistributionHandler.h>
#include <monitoring/IPCHandler.h>
#include <monitoring/MonitorConnector.h>
#include <options/Options.h>
#include <socket/PacketHandler.h>
#include <socket/PFringHandler.h>
#include <socket/ZMQHandler.h>
#include <unistd.h>
#include <utils/LoggingHandler.hpp>
#include <socket/ZMQHandler.h>
#include <csignal>
#include <iostream>
#include <vector>

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
	Options::Initialize(argc, argv);

	InitializeLogging(argv);

	ZMQHandler::Initialize();

	PFringHandler pfRingHandler("dna0");

	SourceIDManager::Initialize();

	PacketHandler::Initialize();

	StorageHandler::Initialize();

	EventBuilder::Initialize();

	/*
	 * Monitor
	 */
	LOG(INFO)<<"Starting Monitoring Services";
	monitoring::MonitorConnector monitor;
	IPCHandler::updateState(RUNNING);
	monitor.startThread();

	/*
	 * Packet Handler
	 */
	unsigned int numberOfPacketHandler = PFringHandler::GetNumberOfQueues();
	std::cout << "Starting " << numberOfPacketHandler
			<< " PacketHandler threads" << std::endl;

	for (unsigned int i = 0; i < numberOfPacketHandler; i++) {
		packetHandlers.push_back(new PacketHandler());
		LOG(INFO)<< "Binding PacketHandler " << i << " to core " << i << "!";
		packetHandlers[i]->startThread(i, i, 15);
	}

	/*
	 * Event Builder
	 */
	unsigned int numberOfEB = Options::GetInt(OPTION_NUMBER_OF_EBS);
	LOG(INFO)<< "Starting " << numberOfEB << " EventBuilder threads"
	<< std::endl;

	for (unsigned int i = 0; i < numberOfEB; i++) {
		eventBuilders.push_back(new EventBuilder());
		eventBuilders[i]->startThread(i, -1, 15);
	}

	/*
	 * L1 Distribution handler
	 */
	cream::L1DistributionHandler l1Handler;
	l1Handler.startThread();

	AExecutable::JoinAll();
	return 0;
}
