/*
 * HandleFrameTask.cpp
 *
 *  Created on: Jun 27, 2014
 *      Author: Jonas Kunze (kunze.jonas@gmail.com)
 */

#include "L1Builder.h"

#include <arpa/inet.h>
#include <eventBuilding/Event.h>
#include <eventBuilding/EventPool.h>
#include <eventBuilding/SourceIDManager.h>
#include <glog/logging.h>
#include <l0/MEPFragment.h>
#include <l1/L1TriggerProcessor.h>
#include <LKr/L1DistributionHandler.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <options/Options.h>
#include <socket/EthernetUtils.h>
#include <socket/NetworkHandler.h>
#include <structs/Network.h>
#include <algorithm>
#include <cstdbool>
#include <iostream>
#include <string>


#include "../options/MyOptions.h"
#include "../socket/HandleFrameTask.h"
#include "L2Builder.h"

namespace na62 {

std::atomic<uint64_t>* L1Builder::L1Triggers_ = new std::atomic<uint64_t>[0xFF
		+ 1];

void L1Builder::buildEvent(l0::MEPFragment* fragment, uint32_t burstID) {
	Event *event = EventPool::GetEvent(fragment->getEventNumber());

	/*
	 * If the event number is too large event is null and we have to drop the data
	 */
	if (event == nullptr) {
		delete fragment;
		return;
	}

	/*
	 * Add new packet to Event
	 */
	if (!event->addL0Event(fragment, burstID)) {
		return;
	} else {
		/*
		 * This event is complete -> process it
		 */
		processL1(event);
	}
}

void L1Builder::processL1(Event *event) {
	if (event->isLastEventOfBurst()) {
		sendEOBBroadcast(event->getEventNumber(), event->getBurstID());
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
			L2Builder::processL2(event);
		}
	}

	/*
	 * If the Event has been rejected by L1 we can destroy it now
	 */
	if (L0L1Trigger == 0) {
		EventPool::FreeEvent(event);
	}
}

void L1Builder::sendL1RequestToCREAMS(Event* event) {
	cream::L1DistributionHandler::Async_RequestLKRDataMulticast(event,
	false);
}

void L1Builder::sendEOBBroadcast(uint32_t eventNumber,
		uint32_t finishedBurstID) {
	LOG(INFO)<<"Sending EOB broadcast to "
	<< Options::GetString(OPTION_EOB_BROADCAST_IP) << ":"
	<< Options::GetInt(OPTION_EOB_BROADCAST_PORT);
	const char* buff = new char[sizeof(EOB_FULL_FRAME)];
	EOB_FULL_FRAME* EOBPacket = (struct EOB_FULL_FRAME*) buff;
	EOBPacket->finishedBurstID = finishedBurstID;
	EOBPacket->lastEventNum = eventNumber;
	EthernetUtils::GenerateUDP(buff,
			EthernetUtils::StringToMAC("FF:FF:FF:FF:FF:FF"),
			inet_addr(Options::GetString(OPTION_EOB_BROADCAST_IP).data()),
			Options::GetInt(OPTION_EOB_BROADCAST_PORT),
			Options::GetInt(OPTION_EOB_BROADCAST_PORT));
	EOBPacket->udp.setPayloadSize(
			sizeof(struct EOB_FULL_FRAME) - sizeof(struct UDP_HDR));
	EOBPacket->udp.ip.check = 0;
	EOBPacket->udp.ip.check = EthernetUtils::GenerateChecksum(
			(const char*) (&EOBPacket->udp.ip), sizeof(struct iphdr));
	EOBPacket->udp.udp.check = EthernetUtils::GenerateUDPChecksum(&EOBPacket->udp,
			sizeof(struct EOB_FULL_FRAME));

	DataContainer container = {(char*)buff, sizeof(struct EOB_FULL_FRAME)};
	NetworkHandler::AsyncSendFrame( std::move(container));

	HandleFrameTask::setNextBurstId(finishedBurstID + 1);
}

}
/* namespace na62 */
