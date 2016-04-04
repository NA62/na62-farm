/*
 * HandleFrameTask.cpp
 *
 *  Created on: Jun 27, 2014
 *      Author: Jonas Kunze (kunze.jonas@gmail.com)
 */

#include "L1Builder.h"

#include <eventBuilding/Event.h>
#include <eventBuilding/EventPool.h>
#include <eventBuilding/SourceIDManager.h>
#include <l0/MEP.h>
#include <l0/MEPFragment.h>
#include <l0/Subevent.h>
#include <l1/L1DistributionHandler.h>
#include <l1/L1Fragment.h>
#include <l1/L1TriggerProcessor.h>
#include <sys/types.h>
#include <cstdbool>

#include "L2Builder.h"

namespace na62 {

std::atomic<uint64_t>* L1Builder::L1Triggers_ = new std::atomic<uint64_t>[0xFF
		+ 1];

std::atomic<uint64_t> L1Builder::L1InputEvents_(0);
std::atomic<uint64_t> L1Builder::L1InputEventsPerBurst_(0);

std::atomic<uint64_t> L1Builder::L1AcceptedEvents_(0);

std::atomic<uint64_t> L1Builder::L1BypassedEvents_(0);

std::atomic<uint64_t> L1Builder::L1Requests_(0);

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
		LOG_ERROR << "Eliminating " << (int)(fragment->getEventNumber()) << " from source " << std::hex << (int)(fragment->getSourceID())
		                                << ":" << (int)(fragment->getSourceSubID()) << std::dec;

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
	if (event->addL0Fragment(fragment, burstID)) {

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
//		LOG_INFO<< "L1Event number before adding 1 " << L1InputEvents_ << ENDL;
		L1InputEvents_.fetch_add(1, std::memory_order_relaxed);
//		LOG_INFO<< "L1Event number after adding 1 " << L1InputEvents_ << ENDL;
		L1InputEventsPerBurst_.fetch_add(1, std::memory_order_relaxed);
		/*
		 * This event is complete -> process it
		 */

		//L1 Input Reduction
		if ((L1InputEvents_ % reductionFactor_ != 0)
				&& (!event->isSpecialTriggerEvent() && (!event->isLastEventOfBurst()))
				//&& (!L1TriggerProcessor::bypassEvent())
				) {
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
	 * Process Level 1 trigger
	 */
	uint_fast8_t l1FlagTrigger = 0;

	if (!event->isSpecialTriggerEvent()) {
		if ((l1FlagMask_ & l0TriggerFlags) || (L1InputEvents_ % autoFlagFactor_ == 0)) {
			l1FlagTrigger = 1;
		} else {
			l1FlagTrigger = 0;
		}
	}
	uint_fast8_t l1TriggerTypeWord = L1TriggerProcessor::compute(event);
	l1TriggerTypeWord = (l1FlagTrigger << 7) | l1TriggerTypeWord;

	uint_fast16_t L0L1Trigger(l0TriggerTypeWord | l1TriggerTypeWord << 8);

	L1Triggers_[l1TriggerTypeWord].fetch_add(1, std::memory_order_relaxed); // The second 8 bits are the L1 trigger type word
	event->setL1Processed(L0L1Trigger);
	uint L1ProcessingTimeIndex = (uint) event->getL1ProcessingTime() / 10.;
	if (L1ProcessingTimeIndex >= 0x64)
		L1ProcessingTimeIndex = 0x64;
	uint EventTimestampIndex = (uint) ((event->getTimestamp() * 25e-08) / 2);
	if (EventTimestampIndex >= 0x64)
		EventTimestampIndex = 0x64;
	L1ProcessingTimeVsEvtNumber_[L1ProcessingTimeIndex][EventTimestampIndex].fetch_add(
			1, std::memory_order_relaxed);
	L1ProcessingTimeCumulative_.fetch_add(event->getL1ProcessingTime(),
			std::memory_order_relaxed);
	if (event->getL1ProcessingTime() >= L1ProcessingTimeMax_)
		L1ProcessingTimeMax_ = event->getL1ProcessingTime();


	// KTAG special
	uint_fast8_t l1KTAGtrigger_mask = 4;
	if (l1FlagTrigger || (l1TriggerTypeWord & l1KTAGtrigger_mask)) {

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

			if (SourceIDManager::NUMBER_OF_EXPECTED_L1_PACKETS_PER_EVENT != 0) {
				/*
				 * Only request accepted events from L1 detectors
				 */

				sendL1Request(event);
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

void L1Builder::sendL1Request(Event* event) {

// See https://github.com/NA62/na62-trigger-algorithms/wiki/CREAM-data
	l1::L1DistributionHandler::Async_RequestL1DataMulticast(event,
			event->isRrequestZeroSuppressedCreamData()
					&& requestZSuppressedLkrData_);
	L1Requests_.fetch_add(1, std::memory_order_relaxed);
}

}
/* namespace na62 */
