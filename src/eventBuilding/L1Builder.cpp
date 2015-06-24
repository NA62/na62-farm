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
#include <l0/Subevent.h>
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
#include <structs/L0TPHeader.h>

#include "../options/MyOptions.h"
#include "../socket/HandleFrameTask.h"
#include "L2Builder.h"

namespace na62 {

std::atomic<uint64_t>* L1Builder::L1Triggers_ = new std::atomic<uint64_t>[0xFF
		+ 1];

std::atomic<uint64_t> L1Builder::L1InputEvents_(0);

std::atomic<uint64_t> L1Builder::L1RequestToCreams_(0);

bool L1Builder::requestZSuppressedLkrData_;

uint L1Builder::reductionFactor_ = 0;

bool L1Builder::L1_flag_mode_ = false;

bool L1Builder::buildEvent(l0::MEPFragment* fragment, uint_fast32_t burstID) {
	Event *event = EventPool::getEvent(fragment->getEventNumber());

	/*
	 * If the event number is too large event is null and we have to drop the data
	 */
	if (event == nullptr) {
		delete fragment;
		return false;
	}

	// L1 Input reduction
	if (fragment->getEventNumber() % reductionFactor_ != 0) {
		delete fragment;
		return false;
	}

	/*
	 * Add new packet to Event
	 */
	if (event->addL0Event(fragment, burstID)) {
		/*
		 * This event is complete -> process it
		 */
		processL1(event);
		return true;
	}
	return false;
}

void L1Builder::processL1(Event *event) {
	uint_fast8_t l0TriggerTypeWord = 1;
	if (SourceIDManager::L0TP_ACTIVE) {
		l0::MEPFragment* L0TPEvent = event->getL0TPSubevent()->getFragment(0);
		L0TpHeader* L0TPData = (L0TpHeader*) L0TPEvent->getPayload();
		event->setFinetime(L0TPData->refFineTime);

		l0TriggerTypeWord = L0TPData->l0TriggerType;
	}

	/*
	 * Store the global event timestamp taken from the reverence detector
	 */
	l0::MEPFragment* tsFragment = event->getL0SubeventBySourceIDNum(
			SourceIDManager::TS_SOURCEID_NUM)->getFragment(0);
	event->setTimestamp(tsFragment->getTimestamp());

	/*
	 * Process Level 1 trigger
	 */
	L1InputEvents_.fetch_add(1, std::memory_order_relaxed);
	uint_fast8_t l1TriggerTypeWord = L1TriggerProcessor::compute(event);
	uint_fast16_t L0L1Trigger(l0TriggerTypeWord | l1TriggerTypeWord << 8);

	L1Triggers_[l1TriggerTypeWord].fetch_add(1, std::memory_order_relaxed); // The second 8 bits are the L1 trigger type word
	event->setL1Processed(L0L1Trigger);

	//if ((l1TriggerTypeWord != 0) || L1_flag_mode_) {
	if (l1TriggerTypeWord != 0) {
		if (SourceIDManager::NUMBER_OF_EXPECTED_CREAM_PACKETS_PER_EVENT != 0) {
			/*
			 * Only request accepted events from LKr
			 */
			sendL1RequestToCREAMS(event);
		} else {
			L2Builder::processL2(event);
		}
	} else {
		/*
		 * If the Event has been rejected by L1 we can destroy it now
		 */
		EventPool::freeEvent(event);
	}

}

void L1Builder::sendL1RequestToCREAMS(Event* event) {
	// Request non zero suppressed LKr data if either the requestZSuppressedLkrData_ is set or
	// See https://github.com/NA62/na62-trigger-algorithms/wiki/CREAM-data
	cream::L1DistributionHandler::Async_RequestLKRDataMulticast(event,
			event->isRrequestZeroSuppressedCreamData()
					&& requestZSuppressedLkrData_);
	L1RequestToCreams_.fetch_add(1, std::memory_order_relaxed);
}

}
/* namespace na62 */
