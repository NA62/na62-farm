/*
 * CommandConnector.cpp
 *
 *  Created on: Jul 25, 2012
 *      Author: Jonas Kunze (kunze.jonas@gmail.com)
 */

#include "CommandConnector.h"

#include <monitoring/IPCHandler.h>

#ifdef USE_GLOG
#include <glog/logging.h>
#endif
#include <algorithm>
#include <cctype>
#include <cstdbool>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>
#include <boost/algorithm/string.hpp>

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

#ifdef USE_GLOG
		LOG(INFO)<< "Received command: " << message;
#endif
		std::transform(message.begin(), message.end(), message.begin(),
				::tolower);

		std::vector<std::string> strings;
		boost::split(strings, message, boost::is_any_of(":"));
		if (strings.size() != 2) {
#ifdef USE_GLOG
			LOG(INFO)<<"Unknown command: " << message;
#endif
		} else {
			std::string command = strings[0];
			if (command == "eob_timestamp") {
				if(MyOptions::GetBool(OPTION_INCREMENT_BURST_AT_EOB)) {
					uint32_t burst = HandleFrameTask::getCurrentBurstId()+1;
					HandleFrameTask::setNextBurstId(burst);
#ifdef USE_GLOG
					LOG(INFO) << "Got EOB time: Incrementing burstID to" << burst;
#endif
				}
			} else if (command == "updatenextburstid") {
				if(!MyOptions::GetBool(OPTION_INCREMENT_BURST_AT_EOB)) {
					uint32_t burst = atoi(strings[1].c_str());
#ifdef USE_GLOG
					LOG(INFO) << "Received new burstID: " << burst;
					HandleFrameTask::setNextBurstId(burst);
#endif
				}
			}
		}
	}
}
}
/* namespace na62 */
