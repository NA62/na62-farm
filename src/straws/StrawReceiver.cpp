/*
 * StrawReceiver.cpp
 *
 *  Created on: May 27, 2014
 *      Author: Jonas Kunze (kunze.jonas@gmail.com)
 */

#include "StrawReceiver.h"

#include <asm-generic/errno-base.h>
#include <glog/logging.h>
#include <socket/EthernetUtils.h>
#include <socket/ZMQHandler.h>
#include <zmq.h>
#include <zmq.hpp>
#include <sstream>
#include <string>

#include "../eventBuilding/StorageHandler.h"
#include "../options/MyOptions.h"

namespace na62 {

tbb::spin_mutex StrawReceiver::sendMutex_;
zmq::socket_t* StrawReceiver::mergerSocket_;

StrawReceiver::StrawReceiver() {
}

StrawReceiver::~StrawReceiver() {
}

std::string StrawReceiver::getZmqAddress() {
	std::stringstream address;
	address << "tcp://" << Options::GetString(OPTION_MERGER_HOST_NAME) << ":"
			<< Options::GetInt(OPTION_STRAW_ZMQ_PORT);
	return address.str();
}

void StrawReceiver::initialize() {
	mergerSocket_ = ZMQHandler::GenerateSocket(ZMQ_PUSH);
	mergerSocket_->connect(StorageHandler::GetMergerAddress().c_str());
}

void StrawReceiver::onShutDown() {
	ZMQHandler::DestroySocket(mergerSocket_);
}

void StrawReceiver::processFrame(DataContainer&& data) {
	zmq::message_t zmqMessage((void*) data.data, data.length,
			(zmq::free_fn*) ZMQHandler::freeZmqMessage);

	while (ZMQHandler::IsRunning()) {
		tbb::spin_mutex::scoped_lock my_lock(sendMutex_);
		try {
			mergerSocket_->send(zmqMessage);
			break;
		} catch (const zmq::error_t& ex) {
			if (ex.num() != EINTR) { // try again if EINTR (signal caught)
				LOG(ERROR)<<ex.what();

				ZMQHandler::DestroySocket (mergerSocket_);
				return;
			}
		}
	}
}

}
/* namespace na62 */
