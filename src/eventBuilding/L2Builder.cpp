/*
 * HandleFrameTask.cpp
 *
 *  Created on: Jun 27, 2014
 *      Author: Jonas Kunze (kunze.jonas@gmail.com)
 */

#include "L2Builder.h"

#include <eventBuilding/Event.h>
#include <eventBuilding/EventPool.h>
#include <l0/MEPFragment.h>
#include <l0/MEP.h>
#include <l0/Subevent.h>
#include <l2/L2Fragment.h>
#include <l2/L2TriggerProcessor.h>
#include <LKr/LkrFragment.h>
#include <netinet/ip.h>
#include <structs/Network.h>
#include <sys/types.h>
#include <cstdbool>

#include "StorageHandler.h"

namespace na62 {

std::atomic<uint64_t>* L2Builder::L2Triggers_ = new std::atomic<uint64_t>[0xFF
		+ 1];
std::atomic<uint64_t> L2Builder::L2InputEvents_(0);
std::atomic<uint64_t> L2Builder::L2InputEventsPerBurst_(0);

std::atomic<uint64_t> L2Builder::L2AcceptedEvents_(0);

std::atomic<uint64_t> L2Builder::L1BuildingTimeCumulative_(0);
std::atomic<uint64_t> L2Builder::L1BuildingTimeMax_(0);
std::atomic<uint64_t> L2Builder::L2ProcessingTimeCumulative_(0);
std::atomic<uint64_t> L2Builder::L2ProcessingTimeMax_(0);
std::atomic<uint64_t> L2Builder::BytesSentToStorage_(0);

std::atomic<uint64_t> L2Builder::EventsSentToStorage_(0);

std::atomic<uint64_t>** L2Builder::L1BuildingTimeVsEvtNumber_;
std::atomic<uint64_t>** L2Builder::L2ProcessingTimeVsEvtNumber_;
uint L2Builder::reductionFactor_ = 0;

uint L2Builder::downscaleFactor_ = 0;

bool L2Builder::buildEvent(cream::LkrFragment* fragment) {
	Event *event = EventPool::getEvent(fragment->getEventNumber());

	/*
	 * If the event number is too large event is null and we have to drop the data
	 */
	if (event == nullptr) {
		delete fragment;
		return false;
	}

	const UDP_HDR* etherFrame =
			reinterpret_cast<const UDP_HDR*>(fragment->getRawData().data);

	// L2 Input reduction
//	if (fragment->getEventNumber() % reductionFactor_ != 0) {
//		delete fragment;
//		return false;
//	}

	/*
	 * Add new packet to EventCollector
	 */
	if (event->addLkrFragment(fragment, etherFrame->ip.saddr)) {
#ifdef MEASURE_TIME
//		LOG_INFO<< "L1BuildingTime " << event->getL1BuildingTime() << ENDL;
//		LOG_INFO<< "EventTimeStamp " << event->getTimestamp()<< ENDL;
		uint L1BuildingTimeIndex = (uint) event->getL1BuildingTime() / 10000.;
		if (L1BuildingTimeIndex >= 0x64)
			L1BuildingTimeIndex = 0x64;
		uint EventTimestampIndex = (uint) ((event->getTimestamp() * 25e-08) / 2);
		if (EventTimestampIndex >= 0x64)
			EventTimestampIndex = 0x64;
//		LOG_INFO<< "[L1BuildingTimeIndex,EventTimeStampIndex] " << L1BuildingTimeIndex << " " << EventTimestampIndex << ENDL;
		L1BuildingTimeVsEvtNumber_[L1BuildingTimeIndex][EventTimestampIndex].fetch_add(
				1, std::memory_order_relaxed);
//		LOG_INFO<< L1BuildingTimeVsEvtNumber_[L1BuildingTimeIndex][EventTimestampIndex] << ENDL;
//		LOG_INFO<< "L1BuildingTime " << event->getL1BuildingTime() << ENDL;
//		LOG_INFO<< "L1BuildingTimeMax (before comparison)" << L1BuildingTimeMax_ << ENDL;
		L1BuildingTimeCumulative_.fetch_add(event->getL1BuildingTime(),
				std::memory_order_relaxed);
//		LOG_INFO<< "L1BuildingTimeCumulative_ " << L1BuildingTimeCumulative_ << ENDL;
		if (event->getL0BuildingTime() >= L1BuildingTimeMax_)
			L1BuildingTimeMax_ = event->getL1BuildingTime();
//		LOG_INFO<< "L0BuildingTimeMax (after comparison)" << L1BuildingTimeMax_ << ENDL;

		L2InputEvents_.fetch_add(1, std::memory_order_relaxed);
		L2InputEventsPerBurst_.fetch_add(1, std::memory_order_relaxed);
#endif
		/*
		 * This event is complete -> process it
		 */

		if ((L2InputEvents_ % reductionFactor_ != 0)
				&& !event->isSpecialTriggerEvent()
				&& (!L2TriggerProcessor::bypassEvent())) {
			EventPool::freeEvent(event);
			//return false;
		} else {
			processL2(event);

			return true;
		}
	}
	return false;
}

void L2Builder::processL2(Event *event) {
	/*
	 * Prepare L2 Data Block to store useful info
	 *
	 */
	l0::MEPFragment* L2Fragment = event->getL2Subevent()->getFragment(0);
	L2_BLOCK* l2Block = (L2_BLOCK*) L2Fragment->getPayload();

	//	const l0::MEPFragment* const L2Fragment = event->getL2Subevent()->getFragment(0);
	//	const char* payload = L2Fragment->getPayload();
	//	L2_BLOCK * l2Block = (L2_BLOCK *) (payload);

	if (!event->isWaitingForNonZSuppressedLKrData()) {
		/*
		 * L1 already passed but non zero suppressed LKr data not yet requested -> Process Level 2 trigger
		 */
		uint_fast8_t L2Trigger = L2TriggerProcessor::compute(event);
//		l2Block->triggerword = L2Trigger;
#ifdef MEASURE_TIME
//		LOG_INFO<< "L2ProcessingTime " << event->getL2ProcessingTime() << ENDL;
//		LOG_INFO<< "EventTimeStamp " << event->getTimestamp()<< ENDL;
		uint L2ProcessingTimeIndex = (uint) event->getL2ProcessingTime() / 1.;
		if (L2ProcessingTimeIndex >= 0x64)
			L2ProcessingTimeIndex = 0x64;
		uint EventTimestampIndex = (uint) ((event->getTimestamp() * 25e-08) / 2);
		if (EventTimestampIndex >= 0x64)
			EventTimestampIndex = 0x64;
//		LOG_INFO<< "[L2ProcessingTimeIndex,EventTimeStampIndex] " << L2ProcessingTimeIndex << " " << EventTimestampIndex << ENDL;
		L2ProcessingTimeVsEvtNumber_[L2ProcessingTimeIndex][EventTimestampIndex].fetch_add(
				1, std::memory_order_relaxed);
//		LOG_INFO<< L2ProcessingTimeVsEvtNumber_[L2ProcessingTimeIndex][EventTimestampIndex] << ENDL;
#endif
		event->setL2Processed(L2Trigger);
#ifdef MEASURE_TIME
//		LOG_INFO<< "L2ProcessingTime " << event->getL2ProcessingTime() << ENDL;
//		LOG_INFO<< "L2ProcessingTimeMax (before comparison)" << L2ProcessingTimeMax_ << ENDL;
		L2ProcessingTimeCumulative_.fetch_add(event->getL2ProcessingTime(),
				std::memory_order_relaxed);
//		LOG_INFO<< "L2ProcessingTimeCumulative_ " << L2ProcessingTimeCumulative_ << ENDL;
		if (event->getL2ProcessingTime() >= L2ProcessingTimeMax_)
			L2ProcessingTimeMax_ = event->getL2ProcessingTime();
//		LOG_INFO<< "L2ProcessingTimeMax (after comparison)" << L2ProcessingTimeMax_ << ENDL;
#endif
		/*
		 * Event has been processed and saved or rejected -> destroy, don't delete so that it can be reused if
		 * during L2 no non zero suppressed LKr data has been requested
		 */
		if (!event->isWaitingForNonZSuppressedLKrData()) {
			if (event->isL2Accepted()) {
				if (!event->isSpecialTriggerEvent()) {
					L2AcceptedEvents_.fetch_add(1, std::memory_order_relaxed);
				}
				/*
				 * Global L2 downscaling
				 */
				if ((uint) L2AcceptedEvents_ % downscaleFactor_ != 0
						&& (!event->isSpecialTriggerEvent()
								&& !event->isL2Bypassed())) {
				} else {

					/*
					 * Send Event to merger
					 */
					BytesSentToStorage_.fetch_add(
							StorageHandler::SendEvent(event),
							std::memory_order_relaxed);
					EventsSentToStorage_.fetch_add(1,
							std::memory_order_relaxed);
//					L2Triggers_[L2Trigger].fetch_add(1,
//							std::memory_order_relaxed);
				}
				EventPool::freeEvent(event);
			}
			L2Triggers_[L2Trigger].fetch_add(1, std::memory_order_relaxed);
		}
	} else { // Process non zero-suppressed data (not used at the moment!
		// When the implementation will be completed, we need to propagate the L2 downscaling
		uint_fast8_t L2Trigger =
				L2TriggerProcessor::onNonZSuppressedLKrDataReceived(event);

		event->setL2Processed(L2Trigger);
#ifdef MEASURE_TIME
//		LOG_INFO<< "L2ProcessingTime " << event->getL2ProcessingTime() << ENDL;
//		LOG_INFO<< "L2ProcessingTimeMax (before comparison)" << L2ProcessingTimeMax_ << ENDL;
		L2ProcessingTimeCumulative_.fetch_add(event->getL2ProcessingTime(),
				std::memory_order_relaxed);
//		LOG_INFO<< "L2ProcessingTimeCumulative_ " << L2ProcessingTimeCumulative_ << ENDL;
		if (event->getL2ProcessingTime() >= L2ProcessingTimeMax_)
			L2ProcessingTimeMax_ = event->getL2ProcessingTime();
//		LOG_INFO<< "L2ProcessingTimeMax (after comparison)" << L2ProcessingTimeMax_ << ENDL;
#endif
		if (event->isL2Accepted()) {
			if (!event->isSpecialTriggerEvent()) {
				L2AcceptedEvents_.fetch_add(1, std::memory_order_relaxed);
			}
			BytesSentToStorage_.fetch_add(StorageHandler::SendEvent(event),
					std::memory_order_relaxed);
			EventsSentToStorage_.fetch_add(1, std::memory_order_relaxed);
		}
//		l2Block->triggerword = L2Trigger;
		L2Triggers_[L2Trigger].fetch_add(1, std::memory_order_relaxed);
		EventPool::freeEvent(event);
	}
}
}
/* namespace na62 */
