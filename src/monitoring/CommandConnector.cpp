/*
 * CommandConnector.cpp
 *
 *  Created on: Jul 25, 2012
 *      Author: Jonas Kunze (kunze.jonas@gmail.com)
 */

#include "CommandConnector.h"

#include <boost/lexical_cast.hpp>
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

namespace na62 {

CommandConnector::CommandConnector() {
}

CommandConnector::~CommandConnector() {
}

void CommandConnector::thread() {
	std::string message;
	while (true) {
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
			if (command == "updateburstid") {
				uint32_t burst = boost::lexical_cast<int>(strings[1]);
#ifdef USE_GLOG
				LOG(INFO) << "Updating burst to " << burst;
#endif
//				EventBuilder::SetNextBurstID(burst);
			}
		}
	}
}
}
/* namespace na62 */
