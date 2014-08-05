/*
 * EventPool.h
 *
 *  Created on: Jul 1, 2014
 *      Author: Jonas Kunze (kunze.jonas@gmail.com)
 */

#ifndef EVENTPOOL_H_
#define EVENTPOOL_H_

#include <vector>
#include <cstdint>

namespace na62 {
class Event;

class EventPool {
private:
	static std::vector<Event*> events_;
	static std::vector<Event*> unusedEvents_;
public:
	EventPool();
	virtual ~EventPool();

	static Event* GetEvent(uint32_t eventNumber);

	static Event* getNewEvent(uint32_t eventNumber);

};

} /* namespace na62 */

#endif /* EVENTPOOL_H_ */
