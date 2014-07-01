/*
 * EventCollector.cpp
 *
 *  Created on: Dec 6, 2011
 *      Author: Jonas Kunze (kunze.jonas@gmail.com)
 */

#include "EventBuilder.h"

#include <arpa/inet.h>
#include <asm-generic/errno-base.h>
#include <boost/thread/detail/thread.hpp>
#include <glog/logging.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <zmq.h>
#include <zmq.hpp>
#include <cstdbool>
#include <iostream>
#include <new>
#include <string>

#include <exceptions/NA62Error.h>
#include <l0/MEPEvent.h>
#include <LKr/L1DistributionHandler.h>
#include <LKr/LKREvent.h>
#include <socket/EthernetUtils.h>
#include <socket/PFringHandler.h>
#include <structs/Network.h>
#include <structs/Event.h>
#include <eventBuilding/Event.h>
#include <eventBuilding/SourceIDManager.h>

#include <l1/L1TriggerProcessor.h>
#include <l2/L2TriggerProcessor.h>
#include "StorageHandler.h"
#include "../options/MyOptions.h"
#include "../socket/ZMQHandler.h"

namespace na62 {
std::vector<Event*> EventBuilder::unusedEvents_;
std::vector<Event*> EventBuilder::eventPool_;

boost::timer::cpu_timer EventBuilder::EOBReceivedTime_;

uint EventBuilder::NUMBER_OF_EBS = 0;

EventBuilder::EventBuilder() :
		L0Socket_(ZMQHandler::GenerateSocket(ZMQ_PULL)), LKrSocket_(
				ZMQHandler::GenerateSocket(ZMQ_PULL)) {
}

EventBuilder::~EventBuilder() {
	L0Socket_->close();
	LKrSocket_->close();
	delete L0Socket_;
	delete LKrSocket_;
}

void EventBuilder::Initialize() {
	NUMBER_OF_EBS = Options::GetInt(OPTION_NUMBER_OF_EBS);
	L2Triggers_ = new std::atomic<uint64_t>[0xFF + 1];

	for (int i = 0; i <= 0xFF; i++) {
		L1Triggers_[i] = 0;
		L2Triggers_[i] = 0;
	}

	eventPool_.resize(
			Options::GetInt(OPTION_NUMBER_OF_EVENTS_PER_BURST_EXPECTED));
	for (uint i = 0; i != Options::GetInt(
	OPTION_NUMBER_OF_EVENTS_PER_BURST_EXPECTED) / NUMBER_OF_EBS; ++i) {
		unusedEvents_.push_back(new Event(0));
	}
}

tbb::task* EventBuilder::execute() {
	boost::this_thread::interruption_point();

	if (items[0].revents & ZMQ_POLLIN) { // L0 data
		L0Socket_->recv(&message);
		handleL0Data((l0::MEPEvent*) message.data());
	} else if (changeBurstID_) {
		if (EOBReceivedTime_.elapsed().wall > 100E6) {
			LOG(INFO)<< "EB "<<threadNum_ <<" changed burst number to " << burstToBeSet << " (" << (EOBReceivedTime_.elapsed().wall*1E6) << " ms after receiving the EOB)";
			currentBurstID_ = burstToBeSet;
			changeBurstID_ = false;
		}
	}

	if (items[1].revents & ZMQ_POLLIN) { // LKr data
		LKrSocket_->recv(&message);
		handleLKRData((cream::LKREvent*) message.data());
	}
	return nullptr;
}



}
/* namespace na62 */
