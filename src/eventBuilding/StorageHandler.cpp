/*
 * StorageHandler.cpp
 *
 *  Created on: Mar 4, 2014
 \*      Author: Jonas Kunze (kunze.jonas@gmail.com)
 */

#include "StorageHandler.h"

#include <boost/algorithm/string.hpp>
#include <asm-generic/errno-base.h>
#include <eventBuilding/Event.h>
#include <eventBuilding/SourceIDManager.h>
#include <tbb/spin_mutex.h>
#include <sstream>

#include <l0/MEPFragment.h>
#include <l0/Subevent.h>
#include <LKr/LkrFragment.h>
#include <structs/Event.h>
#include <structs/Versions.h>
#include <zmq.h>
#include <zmq.hpp>
#include <cstdbool>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <socket/ZMQHandler.h>
#include <glog/logging.h>

#include "../options/MyOptions.h"
#include <storage/EventSerializer.h>

namespace na62 {

std::vector<zmq::socket_t*> StorageHandler::mergerSockets_;

tbb::spin_mutex StorageHandler::sendMutex_;

std::vector<std::string> StorageHandler::GetMergerAddresses(
		std::string mergerList) {
	std::vector<std::string> mergers;
	boost::split(mergers, mergerList, boost::is_any_of(";,"));

	if (mergers.empty()) {
		LOG_ERROR<< "List of running mergers is empty => Stopping now!"
		<< ENDL;
		;
		exit(1);
	}

	std::vector<std::string> addresses;
	for (std::string host : mergers) {
		std::stringstream address;
		address << "tcp://" << host << ":" << Options::GetInt(OPTION_MERGER_PORT);
		addresses.push_back(address.str());
	}
	return addresses;
}

void StorageHandler::setMergers(std::string mergerList) {
	tbb::spin_mutex::scoped_lock my_lock(sendMutex_);
	for (auto socket : mergerSockets_) {
		ZMQHandler::DestroySocket(socket);
	}
	mergerSockets_.clear();

	for (std::string address : GetMergerAddresses(mergerList)) {
		zmq::socket_t* socket = ZMQHandler::GenerateSocket("StorageHandler", ZMQ_PUSH);
		socket->connect(address.c_str());
		mergerSockets_.push_back(socket);
	}
}

void StorageHandler::initialize() {
	setMergers(Options::GetString(OPTION_MERGER_HOST_NAMES));
}

void StorageHandler::onShutDown() {
	for (auto socket : mergerSockets_) {
		ZMQHandler::DestroySocket(socket);
	}
	mergerSockets_.clear();
}

int StorageHandler::SendEvent(const Event* event) {
	/*
	 * TODO: Use multimessage instead of creating a separate buffer and copying the MEP data into it
	 */
	const EVENT_HDR* data = EventSerializer::SerializeEvent(event);

	/*
	 * Send the event to the merger with a zero copy message
	 */
	zmq::message_t zmqMessage((void*) data, data->length * 4,
			(zmq::free_fn*) ZMQHandler::freeZmqMessage);

	while (ZMQHandler::IsRunning()) {
		tbb::spin_mutex::scoped_lock my_lock(sendMutex_);
		try {
			mergerSockets_[event->getBurstID() % mergerSockets_.size()]->send(zmqMessage);
			break;
		} catch (const zmq::error_t& ex) {
			if (ex.num() != EINTR) { // try again if EINTR (signal caught)
				LOG_ERROR<< ex.what() << ENDL;

				onShutDown();
				return 0;
			}
		}
	}

	return data->length * 4;
}
} /* namespace na62 */
