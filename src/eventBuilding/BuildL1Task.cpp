/*
 * HandleFrameTask.cpp
 *
 *  Created on: Jun 27, 2014
 *      Author: root
 */

#include "BuildL1Task.h"

#include <eventBuilding/Event.h>
#include <l0/MEPEvent.h>
#include <cstdint>

namespace na62 {

BuildL1Task::BuildL1Task(l0::MEPEvent* event) :
		event_(event) {

}

BuildL1Task::~BuildL1Task() {
}

tbb::task* BuildL1Task::execute() {
	/*
	 * Receiver only pushes MEPEVENT::eventNum%EBNum events. To fill all holes in eventPool we need divide by the number of event builder
	 */
	const uint32_t eventPoolIndex = (event_->getEventNumber() / NUMBER_OF_EBS);
	Event *event;

	if (eventPoolIndex >= eventPool_.size()) { // Memory overflow
		eventPool_.resize(eventPoolIndex * 2);
		event = getNewEvent(mepEvent->getEventNumber());
		eventPool_[eventPoolIndex] = event;
	} else {
		event = eventPool_[eventPoolIndex];
		if (event == nullptr) { // An event with a higher eventPoolIndex has been received before this one
			event = getNewEvent(mepEvent->getEventNumber());
			eventPool_[eventPoolIndex] = event;
		}
	}

	/*
	 * Add new packet to Event
	 */
	if (!event->addL0Event(mepEvent, getCurrentBurstID())) {
		return;
	} else {
		/*
		 * This event is complete -> process it
		 */
		processL1(event);
	}

	return nullptr;
}
} /* namespace na62 */
