/*
 * HandleFrameTask.cpp
 *
 *  Created on: Jun 27, 2014
 *      Author: Jonas Kunze (kunze.jonas@gmail.com)
 */

#include "BuildL2Task.h"

#include <bits/atomic_base.h>
#include <eventBuilding/Event.h>
//#include <eventBuilding/SourceIDManager.h>
#include <exceptions/NA62Error.h>
//#include <l1/L1TriggerProcessor.h>
#include <l2/L2TriggerProcessor.h>
#include <LKr/LKREvent.h>

#include "EventPool.h"
#include "StorageHandler.h"

namespace na62 {

std::atomic<uint64_t>* BuildL2Task::L2Triggers_ = new std::atomic<uint64_t>[0xFF
		+ 1];

std::atomic<uint64_t> BuildL2Task::BytesSentToStorage_(0);
std::atomic<uint64_t> BuildL2Task::EventsSentToStorage_(0);

BuildL2Task::BuildL2Task(cream::LKREvent* event) :
		lkrEvent_(event) {
}

BuildL2Task::~BuildL2Task() {
}

tbb::task* BuildL2Task::execute() {
	Event *event = EventPool::GetEvent(lkrEvent_->getEventNumber());

	if (event == nullptr) {
		throw na62::NA62Error(
				"Received an LKrEvent with ID "
						+ std::to_string(lkrEvent_->getEventNumber())
						+ " while there has not been any L0 data received for this event");
	}
	/*
	 * Add new packet to EventCollector
	 */
	if (!event->addLKREvent(lkrEvent_)) {
		// result == false -> subevents are still incomplete
		return nullptr;
	} else {
		// result == true -> Last missing packet received!
		/*
		 * This event is complete -> process it
		 */
		processL2(event);
	}
	return nullptr;
}

void BuildL2Task::processL2(Event *event) {
	if (!event->isWaitingForNonZSuppressedLKrData()) {
		/*
		 * L1 already passed but non zero suppressed LKr data not yet requested -> Process Level 2 trigger
		 */
		uint8_t L2Trigger = L2TriggerProcessor::compute(event);

		event->setL2Processed(L2Trigger);

		/*
		 * Event has been processed and saved or rejected -> destroy, don't delete so that it can be reused if
		 * during L2 no non zero suppressed LKr data has been requested
		 */
		if (!event->isWaitingForNonZSuppressedLKrData()) {
			if (event->isL2Accepted()) {
				BytesSentToStorage_ += StorageHandler::SendEvent(event);
				EventsSentToStorage_++;
			}
			L2Triggers_[L2Trigger]++;
			event->destroy();
		}
	} else {
		uint8_t L2Trigger = L2TriggerProcessor::onNonZSuppressedLKrDataReceived(
				event);

		event->setL2Processed(L2Trigger);
		if (event->isL2Accepted()) {
			BytesSentToStorage_ += StorageHandler::SendEvent(event);
			EventsSentToStorage_++;
		}
		L2Triggers_[L2Trigger]++;
		event->destroy();
	}
}
}
/* namespace na62 */
