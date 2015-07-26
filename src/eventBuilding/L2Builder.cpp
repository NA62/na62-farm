/*
 * HandleFrameTask.cpp
 *
 *  Created on: Jun 27, 2014
 *      Author: Jonas Kunze (kunze.jonas@gmail.com)
 */

#include "L2Builder.h"

#include <eventBuilding/Event.h>
#include <eventBuilding/EventPool.h>
#include <LKr/LkrFragment.h>

#include <l2/L2TriggerProcessor.h>
#include <structs/Network.h>
#include "StorageHandler.h"

namespace na62 {

std::atomic<uint64_t>* L2Builder::L2Triggers_ = new std::atomic<uint64_t>[0xFF
		+ 1];
std::atomic<uint64_t> L2Builder::L2InputEvents_(0);

std::atomic<uint64_t> L2Builder::L2AcceptedEvents_(0);

std::atomic<uint64_t> L2Builder::BytesSentToStorage_(0);

std::atomic<uint64_t> L2Builder::EventsSentToStorage_(0);

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
			reinterpret_cast<const UDP_HDR*>(fragment->getEtherFrame());

	// L2 Input reduction
//	if (fragment->getEventNumber() % reductionFactor_ != 0) {
//		delete fragment;
//		return false;
//	}

	/*
	 * Add new packet to EventCollector
	 */
	if (event->addLkrFragment(fragment, etherFrame->ip.saddr)) {
		L2InputEvents_.fetch_add(1, std::memory_order_relaxed);
		/*
		 * This event is complete -> process it
		 */

		if ((L2InputEvents_ % reductionFactor_ != 0) && !event->isSpecialTriggerEvent() && (!L2TriggerProcessor::bypassEvent())) {
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
	if (!event->isWaitingForNonZSuppressedLKrData()) {
		/*
		 * L1 already passed but non zero suppressed LKr data not yet requested -> Process Level 2 trigger
		 */
		uint_fast8_t L2Trigger = L2TriggerProcessor::compute(event);

		event->setL2Processed(L2Trigger);

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
			L2Triggers_[L2Trigger].fetch_add(1,std::memory_order_relaxed);
		}
	} else { // Process non zero-suppressed data (not used at the moment!
		// When the implementation will be completed, we need to propagate the L2 downscaling
		uint_fast8_t L2Trigger =
				L2TriggerProcessor::onNonZSuppressedLKrDataReceived(event);

		event->setL2Processed(L2Trigger);
		if (event->isL2Accepted()) {
			if (!event->isSpecialTriggerEvent()) {
				L2AcceptedEvents_.fetch_add(1, std::memory_order_relaxed);
			}
			BytesSentToStorage_.fetch_add(StorageHandler::SendEvent(event),
					std::memory_order_relaxed);
			EventsSentToStorage_.fetch_add(1, std::memory_order_relaxed);
		}
		L2Triggers_[L2Trigger].fetch_add(1, std::memory_order_relaxed);
		EventPool::freeEvent(event);
	}
}
}
/* namespace na62 */
