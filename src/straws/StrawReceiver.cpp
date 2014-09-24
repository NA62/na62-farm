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
std::vector<zmq::socket_t*> StrawReceiver::pushSockets_;

StrawReceiver::StrawReceiver() {
}

StrawReceiver::~StrawReceiver() {
}

std::vector<std::string> StrawReceiver::getZmqAddresses() {
	std::vector<std::string> addresses;
	for (std::string host : Options::GetStringList(OPTION_STRAW_ZMQ_DST_HOSTS)) {
		std::stringstream address;
		address << "tcp://" << Options::GetString(OPTION_MERGER_HOST_NAME)
				<< ":" << Options::GetInt(OPTION_STRAW_ZMQ_PORT);
		addresses.push_back(address.str());
	}
	return addresses;
}

void StrawReceiver::initialize() {
	for (std::string address : getZmqAddresses()) {
		zmq::socket_t* socket = ZMQHandler::GenerateSocket(ZMQ_PUSH);
		socket->connect(address.c_str());
		pushSockets_.push_back(socket);
	}
}

void StrawReceiver::onShutDown() {
	for (auto socket : pushSockets_) {
		ZMQHandler::DestroySocket(socket);
	}
}

void StrawReceiver::processFrame(DataContainer&& data, uint burstID) {
	tbb::spin_mutex::scoped_lock my_lock(sendMutex_);
	zmq::socket_t* socket = pushSockets_[burstID % pushSockets_.size()];
	socket->send((void*) &burstID, sizeof(burstID), ZMQ_SNDMORE);

	zmq::message_t zmqMessage((void*) data.data, data.length,
			(zmq::free_fn*) ZMQHandler::freeZmqMessage);

	ZMQHandler::sendMessage(socket, std::move(zmqMessage), 0);
}

}
/* namespace na62 */
