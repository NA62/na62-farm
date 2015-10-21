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
#include <l1/L1Fragment.h>
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
std::atomic<uint64_t> L1Builder::L1InputEventsPerBurst_(0);

std::atomic<uint64_t> L1Builder::L1AcceptedEvents_(0);

std::atomic<uint64_t> L1Builder::L1BypassedEvents_(0);

std::atomic<uint64_t> L1Builder::L1RequestToCreams_(0);

std::atomic<uint64_t> L1Builder::L0BuildingTimeCumulative_(0);
std::atomic<uint64_t> L1Builder::L0BuildingTimeMax_(0);
std::atomic<uint64_t> L1Builder::L1ProcessingTimeCumulative_(0);
std::atomic<uint64_t> L1Builder::L1ProcessingTimeMax_(0);
std::atomic<uint64_t>** L1Builder::L0BuildingTimeVsEvtNumber_;
std::atomic<uint64_t>** L1Builder::L1ProcessingTimeVsEvtNumber_;
bool L1Builder::requestZSuppressedLkrData_;

uint L1Builder::reductionFactor_ = 0;

uint L1Builder::downscaleFactor_ = 0;

uint L1Builder::autoFlagFactor_ = 0;

//bool L1Builder::L1_flag_mode_ = 0;
uint16_t L1Builder::l1FlagMask_ = 0;

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
//	if (fragment->getEventNumber() % reductionFactor_ != 0
//			&& (!event->isSpecialTriggerEvent() && !event->isL1Bypassed())) {
//		delete fragment;
//		return false;
//	}

	/*
	 * Add new packet to Event
	 */
	if (event->addL0Event(fragment, burstID)) {

		/*
		 * Store the global event timestamp taken from the reverence detector
		 */
		l0::MEPFragment* tsFragment = event->getL0SubeventBySourceIDNum(
				SourceIDManager::TS_SOURCEID_NUM)->getFragment(0);
		event->setTimestamp(tsFragment->getTimestamp());

//		LOG_INFO<< "L0BuildingTime " << event->getL0BuildingTime() << ENDL;
//		LOG_INFO<< "EventTimeStamp " << event->getTimestamp()<< ENDL;
		uint L0BuildingTimeIndex = (uint) event->getL0BuildingTime() / 5000.;
		if (L0BuildingTimeIndex >= 0x64)
			L0BuildingTimeIndex = 0x64;
		uint EventTimestampIndex = (uint) ((event->getTimestamp() * 25e-08) / 2);
		if (EventTimestampIndex >= 0x64)
			EventTimestampIndex = 0x64;
//		LOG_INFO<< "[L0BuildingTimeIndex,EventTimeStampIndex] " << L0BuildingTimeIndex << " " << EventTimestampIndex << ENDL;
		L0BuildingTimeVsEvtNumber_[L0BuildingTimeIndex][EventTimestampIndex].fetch_add(
				1, std::memory_order_relaxed);

//		LOG_INFO<< L0BuildingTimeVsEvtNumber_[L0BuildingTimeIndex][EventTimestampIndex] << ENDL;
//		LOG_INFO<< "L0BuildingTime " << event->getL0BuildingTime() << ENDL;
//		LOG_INFO<< "L0BuildingTimeMax (before comparison)" << L0BuildingTimeMax_ << ENDL;
		L0BuildingTimeCumulative_.fetch_add(event->getL0BuildingTime(),
				std::memory_order_relaxed);
//		LOG_INFO<< "L0BuildingTimeCumulative_ " << L0BuildingTimeCumulative_ << ENDL;
		if (event->getL0BuildingTime() >= L0BuildingTimeMax_)
			L0BuildingTimeMax_ = event->getL0BuildingTime();
//		LOG_INFO<< "L0BuildingTimeMax (after comparison)" << L0BuildingTimeMax_ << ENDL;

		event->readTriggerTypeWordAndFineTime();
		L1InputEvents_.fetch_add(1, std::memory_order_relaxed);
		L1InputEventsPerBurst_.fetch_add(1, std::memory_order_relaxed);
		/*
		 * This event is complete -> process it
		 */

		//L1 Input Reduction
		if ((L1InputEvents_ % reductionFactor_ != 0)
				&& (!event->isSpecialTriggerEvent())
				&& (!L1TriggerProcessor::bypassEvent())) {
			EventPool::freeEvent(event);
		} else {
			processL1(event);

			return true;
		}
	}
	return false;
}

void L1Builder::processL1(Event *event) {

	uint_fast8_t l0TriggerTypeWord = event->getL0TriggerTypeWord();
	uint_fast16_t l0TriggerFlags = event->getTriggerFlags();

//	if (SourceIDManager::L0TP_ACTIVE) {
//		l0::MEPFragment* L0TPEvent = event->getL0TPSubevent()->getFragment(0);
//		L0TpHeader* L0TPData = (L0TpHeader*) L0TPEvent->getPayload();
//		event->setFinetime(L0TPData->refFineTime);
//
//		l0TriggerTypeWord = L0TPData->l0TriggerType;
//	}
//	LOG_INFO<< "L0 Trigger word " << (uint)l0TriggerTypeWord << ENDL;
//	LOG_INFO<< "Ref Detector finetime " << (uint)event->getFinetime() << ENDL;

	/*
	 * Store the global event timestamp taken from the reverence detector
	 */
	l0::MEPFragment* tsFragment = event->getL0SubeventBySourceIDNum(
			SourceIDManager::TS_SOURCEID_NUM)->getFragment(0);
	event->setTimestamp(tsFragment->getTimestamp());

	/*
	 * Prepare L1 Data Block to store useful info
	 *
	 */
	l0::MEPFragment* L1Fragment = event->getL1Subevent()->getFragment(0);
	L1_BLOCK* l1Block = (L1_BLOCK*) L1Fragment->getPayload();

	//	const l0::MEPFragment* const L1Fragment = event->getL1Subevent()->getFragment(0);
	//	const char* payload = L1Fragment->getPayload();
	//	L1_BLOCK * l1Block = (L1_BLOCK *) (payload);

	/*
	 * Process Level 1 trigger
	 */
	uint_fast8_t l1FlagTrigger = 0;
//	LOG_INFO<< "l1FlagMask_ " << (uint) l1FlagMask_ << ENDL;
//	LOG_INFO<< "l0TriggerFlags " << (uint) l0TriggerFlags << ENDL;
//	LOG_INFO<< " AND " << (l1FlagMask_ & l0TriggerFlags) << ENDL;
//	LOG_INFO << "IsSpecialEvent? " << event->isSpecialTriggerEvent() << ENDL;
//	if (L1_flag_mode_ || (!L1_flag_mode_ && (L1InputEvents_ % autoFlagFactor_ == 0))) {
	if (!event->isSpecialTriggerEvent()) {
		if ((l1FlagMask_ & l0TriggerFlags)
				|| ((!(l1FlagMask_ & l0TriggerFlags))
						&& (L1InputEvents_ % autoFlagFactor_ == 0))) {
//			LOG_INFO<< "******FLAG!!! " << L1InputEvents_ << " % " << autoFlagFactor_ << ENDL;
			l1FlagTrigger = 1;
		} else {
//			LOG_INFO<< "******CUT!!! *******"<< ENDL;
			l1FlagTrigger = 0;
		}
	}
	uint_fast8_t l1TriggerTypeWord = L1TriggerProcessor::compute(event);
//	LOG_INFO<< "*******l1TriggerTypeWord (before flag) " << (uint)l1TriggerTypeWord << ENDL;
	l1TriggerTypeWord = (l1FlagTrigger << 7) | l1TriggerTypeWord;
//	LOG_INFO<< "*******l1TriggerTypeWord (after flag) " << (uint)l1TriggerTypeWord << ENDL;
	l1Block->triggerword = l1TriggerTypeWord;
	uint_fast16_t L0L1Trigger(l0TriggerTypeWord | l1TriggerTypeWord << 8);

	L1Triggers_[l1TriggerTypeWord].fetch_add(1, std::memory_order_relaxed); // The second 8 bits are the L1 trigger type word
	event->setL1Processed(L0L1Trigger);
//	LOG_INFO<< "L1ProcessingTime " << event->getL1ProcessingTime() << ENDL;
//	LOG_INFO<< "EventTimeStamp " << event->getTimestamp()<< ENDL;
	uint L1ProcessingTimeIndex = (uint) event->getL1ProcessingTime() / 10.;
	if (L1ProcessingTimeIndex >= 0x64)
		L1ProcessingTimeIndex = 0x64;
	uint EventTimestampIndex = (uint) ((event->getTimestamp() * 25e-08) / 2);
	if (EventTimestampIndex >= 0x64)
		EventTimestampIndex = 0x64;
//	LOG_INFO<< "[L1ProcessingTimeIndex,EventTimeStampIndex] " << L1ProcessingTimeIndex << " " << EventTimestampIndex << ENDL;
	L1ProcessingTimeVsEvtNumber_[L1ProcessingTimeIndex][EventTimestampIndex].fetch_add(
			1, std::memory_order_relaxed);
//	LOG_INFO<< L1ProcessingTimeVsEvtNumber_[L1ProcessingTimeIndex][EventTimestampIndex] << ENDL;
//	LOG_INFO<< "L1ProcessingTime " << event->getL1ProcessingTime() << ENDL;
//	LOG_INFO<< "L1ProcessingTimeMax (before comparison)" << L1ProcessingTimeMax_ << ENDL;
	L1ProcessingTimeCumulative_.fetch_add(event->getL1ProcessingTime(),
			std::memory_order_relaxed);
//	LOG_INFO<< "L1ProcessingTimeCumulative_ " << L1ProcessingTimeCumulative_ << ENDL;
	if (event->getL1ProcessingTime() >= L1ProcessingTimeMax_)
		L1ProcessingTimeMax_ = event->getL1ProcessingTime();
//	LOG_INFO<< "L1ProcessingTimeMax (after comparison)" << L1ProcessingTimeMax_ << ENDL;

	if (l1TriggerTypeWord != 0) {

		if (!event->isSpecialTriggerEvent()) {
			L1AcceptedEvents_.fetch_add(1, std::memory_order_relaxed);

			if (event->isL1Bypassed()) {
				L1BypassedEvents_.fetch_add(1, std::memory_order_relaxed);
			}
		}
		//Global L1 downscaling
		if (((uint) L1AcceptedEvents_ % downscaleFactor_ != 0
				&& (!event->isSpecialTriggerEvent() && !event->isL1Bypassed()))) {
			EventPool::freeEvent(event);
		} else {  // Downscaled event

			if (SourceIDManager::NUMBER_OF_EXPECTED_CREAM_PACKETS_PER_EVENT
					!= 0) {
				/*
				 * Only request accepted events from LKr
				 */
				sendL1RequestToCREAMS(event);
			} else {
				L2Builder::processL2(event);
			}
		}
	} else { // Event not accepted
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
