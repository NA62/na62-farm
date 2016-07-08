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
#include <sstream>

//#include <structs/Event.h>
//#include <structs/Versions.h>
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
tbb::concurrent_queue<const EVENT_HDR*> StorageHandler::DataQueue_;

std::recursive_mutex StorageHandler::sendMutex_;

void StorageHandler::setMergers(std::vector<std::string> mergerList) {
	std::lock_guard<std::recursive_mutex> my_lock(sendMutex_);
	for (auto socket : mergerSockets_) {
		ZMQHandler::DestroySocket(socket);
	}
	mergerSockets_.clear();

	for (auto host : mergerList) {
		try {
			std::stringstream address;
			address << "tcp://"<< host << ":" << Options::GetInt(OPTION_MERGER_PORT);
			zmq::socket_t* socket = ZMQHandler::GenerateSocket("StorageHandler", ZMQ_PUSH);
			socket->connect(address.str().c_str());
			mergerSockets_.push_back(socket);
		} catch (const zmq::error_t& ex) {
			LOG_ERROR("Failed to initialize ZMQ for merger " << host << " because: " << ex.what());
			throw ex;
		}
	}
}

void StorageHandler::initialize() {
	setMergers(Options::GetStringList(OPTION_MERGER_HOST_NAMES));
}

void StorageHandler::onShutDown() {
	std::lock_guard<std::recursive_mutex> my_lock(sendMutex_);
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
	int dataLength = data->length * 4;

	StorageHandler::DataQueue_.push(data);
	return dataLength;
}

void StorageHandler::thread() {
	while (running_) {
		const EVENT_HDR* data;
		if (StorageHandler::DataQueue_.try_pop(data)) {
			try {
				zmq::message_t zmqMessage((void*) data, data->length * 4,
						(zmq::free_fn*) ZMQHandler::freeZmqMessage);


				//	LOG_ERROR ("Send to merger burst "<<event->getBurstID() << " number of mergers " << mergerSockets_.size());
				while (ZMQHandler::IsRunning()) {
					std::lock_guard<std::recursive_mutex> my_lock(sendMutex_);
					try {
						mergerSockets_[data->burstID % mergerSockets_.size()]->send(zmqMessage);
						break;
					} catch (const zmq::error_t& ex) {
						if (ex.num() != EINTR) { // try again if EINTR (signal caught)
							LOG_ERROR(ex.what());
							try {
								initialize(); //try to re-initialize mergers
							} catch (const zmq::error_t& exx) {
								LOG_ERROR(exx.what());
							}
						}
					}
				}
			} catch (const zmq::error_t& e) {
				LOG_ERROR("Failed to create ZMQ message, because: " << e.what());
			}
		}
		else {
			boost::this_thread::sleep(boost::posix_time::microsec(50));
		}
	}
}
void StorageHandler::onInterruption() {
		running_ = false;
		}
} /* namespace na62 */
