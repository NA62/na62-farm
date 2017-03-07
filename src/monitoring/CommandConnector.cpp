/*
 * CommandConnector.cpp
 *
 *  Created on: Jul 25, 2012
 *      Author: Jonas Kunze (kunze.jonas@gmail.com)
 */

#include "CommandConnector.h"

#include <monitoring/IPCHandler.h>

#include <algorithm>
#include <cctype>
#include <cstdbool>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>
#include <boost/algorithm/string.hpp>
#include <monitoring/BurstIdHandler.h>
#include <monitoring/HltStatistics.h>

#include "../eventBuilding/StorageHandler.h"
#include "../options/MyOptions.h"
#include "../socket/PacketHandler.h"
#include "../eventBuilding/L1Builder.h"
#include "../eventBuilding/L2Builder.h"
#include <l1/L1TriggerProcessor.h>
#include <l2/L2TriggerProcessor.h>
#include <socket/NetworkHandler.h>

namespace na62 {

CommandConnector::CommandConnector() {
}

CommandConnector::~CommandConnector() {
}

void CommandConnector::thread() {
	std::string message;
	while (IPCHandler::isRunning()) {
		/*
		 * Synchronious receive:
		 */
		message = IPCHandler::getNextCommand();
		if (message == "") {
			continue;
		}

		LOG_INFO("Received command: " << message);
		std::transform(message.begin(), message.end(), message.begin(), ::tolower);

		std::vector<std::string> strings;
		boost::split(strings, message, boost::is_any_of(":"));

		if (strings.size() != 2) {
			LOG_ERROR("Unknown command received: " << message);
			continue;
		}

		std::string command = strings[0];
		if (command == "eob_timestamp") {
			if (MyOptions::GetBool(OPTION_INCREMENT_BURST_AT_EOB)) {
				uint_fast32_t burst = BurstIdHandler::getCurrentBurstId() + 1;
				BurstIdHandler::setNextBurstID(burst);
				LOG_INFO("Got EOB time: Incrementing burstID to" << burst);
			}
		} else if (command == "updatenextburstid") {
			if (!MyOptions::GetBool(OPTION_INCREMENT_BURST_AT_EOB)) {
				uint_fast32_t burst = atoi(strings[1].c_str());
				LOG_INFO("Received new burstID: " << burst);
				BurstIdHandler::setNextBurstID(burst);
			}
		} else {
			LOG_INFO("Ignore command received: " << message);
		}
	}
}
}
/* namespace na62 */
