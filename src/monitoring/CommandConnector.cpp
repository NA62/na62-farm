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

#include "../eventBuilding/StorageHandler.h"
#include "../options/MyOptions.h"
#include "../socket/HandleFrameTask.h"

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

		LOG_INFO<< "Received command: " << message << ENDL;
		std::transform(message.begin(), message.end(), message.begin(),
				::tolower);

		std::vector<std::string> strings;
		boost::split(strings, message, boost::is_any_of(":"));
		if (strings.size() != 2) {
			LOG_INFO<<"Unknown command: " << message << ENDL;
		} else {
			std::string command = strings[0];
			if (command == "eob_timestamp") {
				if(MyOptions::GetBool(OPTION_INCREMENT_BURST_AT_EOB)) {
					uint32_t burst = HandleFrameTask::getCurrentBurstId()+1;
					HandleFrameTask::setNextBurstId(burst);
					LOG_INFO << "Got EOB time: Incrementing burstID to" << burst << ENDL;
				}
			} else if (command == "updatenextburstid") {
				if(!MyOptions::GetBool(OPTION_INCREMENT_BURST_AT_EOB)) {
					uint32_t burst = atoi(strings[1].c_str());
					LOG_INFO << "Received new burstID: " << burst << ENDL;
					HandleFrameTask::setNextBurstId(burst);
				}
			} else if (command == "runningmergers") {
				std::string mergerList=strings[1];
				StorageHandler::setMergers(mergerList);
			}
		}
	}
}
}
/* namespace na62 */
