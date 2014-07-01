/*
 * HandleFrameTask.cpp
 *
 *  Created on: Jun 27, 2014
 *      Author: Jonas Kunze (kunze.jonas@gmail.com)
 */

#include "BuildL1Task.h"

#include <arpa/inet.h>
#include <eventBuilding/Event.h>
#include <eventBuilding/SourceIDManager.h>
#include <glog/logging.h>
#include <l0/MEPEvent.h>
#include <l1/L1TriggerProcessor.h>
#include <LKr/L1DistributionHandler.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <options/Options.h>
#include <socket/EthernetUtils.h>
#include <socket/PFringHandler.h>
#include <structs/Event.h>
#include <structs/Network.h>
#include <cstdbool>
#include <cstdint>
#include <iostream>
#include <string>

#include "EventBuilder.h"

namespace na62 {

std::atomic<uint64_t>* BuildL1Task::L1Triggers_ = new std::atomic<uint64_t>[0xFF
		+ 1];

BuildL1Task::BuildL1Task(l0::MEPEvent* event) :
		mepEvent_(event) {

}

BuildL1Task::~BuildL1Task() {
}

tbb::task* BuildL1Task::execute() {
	/*
	 * Receiver only pushes MEPEVENT::eventNum%EBNum events. To fill all holes in eventPool we need divide by the number of event builder
	 */
	Event *event = EventPool::GetEvent(mepEvent_->getEventNumber());

	/*
	 * Add new packet to Event
	 */
	if (!event->addL0Event(mepEvent_, getCurrentBurstID())) {
		return nullptr;
	} else {
		/*
		 * This event is complete -> process it
		 */
		processL1(event);
	}

	return nullptr;
}

void BuildL1Task::processL1(Event *event) {
	if (event->isLastEventOfBurst()) {
		sendEOBBroadcast(event->getEventNumber(), getCurrentBurstID());
	}

	/*
	 * Process Level 1 trigger
	 */
	uint16_t L0L1Trigger = L1TriggerProcessor::compute(event);
	L1Triggers_[L0L1Trigger >> 8]++; // The second 8 bits are the L1 trigger type word
	event->setL1Processed(L0L1Trigger);

	if (SourceIDManager::NUMBER_OF_EXPECTED_CREAM_PACKETS_PER_EVENT != 0) {
		if (L0L1Trigger != 0) {
			/*
			 * Only request accepted events from LKr
			 */
			sendL1RequestToCREAMS(event);
		}
	} else {
		if (L0L1Trigger != 0) {
			BuildL2Task::processL2(event);
		}
	}

	/*
	 * If the Event has been rejected by L1 we can destroy it now
	 */
	if (L0L1Trigger == 0) {
		event->destroy();
	}
}

void BuildL1Task::sendL1RequestToCREAMS(Event* event) {
	cream::L1DistributionHandler::Async_RequestLKRDataMulticast(event,
	false);
}

void BuildL1Task::sendEOBBroadcast(uint32_t eventNumber,
		uint32_t finishedBurstID) {
	LOG(INFO)<<"Sending EOB broadcast to "
	<< Options::GetString(OPTION_EOB_BROADCAST_IP) << ":"
	<< Options::GetInt(OPTION_EOB_BROADCAST_PORT);
	EOB_FULL_FRAME EOBPacket;

	EOBPacket.finishedBurstID = finishedBurstID;
	EOBPacket.lastEventNum = eventNumber;

	EthernetUtils::GenerateUDP((const char*) &EOBPacket,
			EthernetUtils::StringToMAC("FF:FF:FF:FF:FF:FF"),
			inet_addr(Options::GetString(OPTION_EOB_BROADCAST_IP).data()),
			Options::GetInt(OPTION_EOB_BROADCAST_PORT),
			Options::GetInt(OPTION_EOB_BROADCAST_PORT));

	EOBPacket.udp.setPayloadSize(
			sizeof(struct EOB_FULL_FRAME) - sizeof(struct UDP_HDR));
	EOBPacket.udp.ip.check = 0;
	EOBPacket.udp.ip.check = EthernetUtils::GenerateChecksum(
			(const char*) (&EOBPacket.udp.ip), sizeof(struct iphdr));
	EOBPacket.udp.udp.check = EthernetUtils::GenerateUDPChecksum(&EOBPacket.udp,
			sizeof(struct EOB_FULL_FRAME));

	PFringHandler::SendFrame((char*) &EOBPacket, sizeof(struct EOB_FULL_FRAME),
			true, false);

	BuildL1Task::setNextBurstID(EOBPacket.finishedBurstID + 1);
}

}
/* namespace na62 */
