/*
 * EventPool.cpp
 *
 *  Created on: Jul 1, 2014
 *      Author: Jonas Kunze (kunze.jonas@gmail.com)
 */

#include "EventPool.h"

#include <eventBuilding/Event.h>
#include <options/Options.h>
#include <tbb/tbb.h>
#include <glog/logging.h>

#include "../options/MyOptions.h"

namespace na62 {

std::vector<Event*> EventPool::events_;
uint32_t EventPool::numberOfEventsStored_;

void EventPool::Initialize() {
	numberOfEventsStored_ = Options::GetInt(
	OPTION_MAX_NUMBER_OF_EVENTS_PER_BURST);
	events_.resize(numberOfEventsStored_);

	/*
	 * Fill the pool with empty events. Do it with parallel_for using tbb
	 */
	tbb::parallel_for(tbb::blocked_range<uint32_t>(0, numberOfEventsStored_),
			[](const tbb::blocked_range<uint32_t>& r) {
				for(size_t eventNumber=r.begin();eventNumber!=r.end(); ++eventNumber)
				events_[eventNumber] = new Event(eventNumber);
			});
}

Event* EventPool::GetEvent(uint32_t eventNumber) {
	if (eventNumber >= numberOfEventsStored_) {
		LOG(ERROR) << "Received Event with event number " << eventNumber
				<< " which is higher than configured by --"
				<< OPTION_MAX_NUMBER_OF_EVENTS_PER_BURST << std::endl;
		return nullptr;
	}
	return events_[eventNumber];
}

} /* namespace na62 */
