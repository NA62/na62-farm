/*
 * EventPool.cpp
 *
 *  Created on: Jul 1, 2014
 *      Author: Jonas Kunze (kunze.jonas@gmail.com)
 */

#include "EventPool.h"

#include <eventBuilding/Event.h>

namespace na62 {

std::vector<Event*> EventPool::events_;
std::vector<Event*> EventPool::unusedEvents_;

EventPool::EventPool() {
}

EventPool::~EventPool() {
}

Event* EventPool::GetEvent(uint32_t eventNumber) {
	Event* event;
	if (eventNumber >= events_.size()) { // Memory overflow
		events_.resize(eventNumber * 2);
		event = getNewEvent(eventNumber);
		events_[eventNumber] = event;
	} else {
		event = events_[eventNumber];
		if (event == nullptr) { // An event with a higher eventPoolIndex has been received before this one
			event = getNewEvent(eventNumber);
			events_[eventNumber] = event;
		}
	}
	return event;
}

Event* EventPool::getNewEvent(uint32_t eventNumber) {
	if (!unusedEvents_.empty()) {
		Event *event;
		event = unusedEvents_.back();
		unusedEvents_.pop_back();
		event->setEventNumber(eventNumber);
		return event;
	} else {
		return new Event(eventNumber);
	}
}

} /* namespace na62 */
