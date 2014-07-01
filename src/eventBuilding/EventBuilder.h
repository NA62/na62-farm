/*
 * EventCollector.h
 *
 *  Created on: Dec 6, 2011
 *      Author: Jonas Kunze (kunze.jonas@gmail.com)
 */

#pragma once
#ifndef EVENTBUILDER_H_
#define EVENTBUILDER_H_

#include <boost/timer/timer.hpp>
#ifdef USE_GLOG
	#include <glog/logging.h>
#endif
#include <eventBuilding/Event.h>
#include <sys/types.h>
#include <atomic>
#include <cstdbool>
#include <cstdint>
#include <iostream>
#include <vector>

#include <tbb/task.h>

namespace na62 {
class Event;
class L1TriggerProcessor;
class L2TriggerProcessor;

namespace cream {
class LKREvent;
} /* namespace cream */

namespace l0 {
class MEPFragment;
} /* namespace l0 */
} /* namespace na62 */

namespace zmq {
class socket_t;
} /* namespace zmq */

namespace na62 {

class EventBuilder:  public tbb::task {
public:
	EventBuilder();
	virtual ~EventBuilder();
	static void Initialize();


	static inline const uint64_t GetBytesSentToStorage() {
		return BytesSentToStorage_;
	}

	static inline const uint64_t GetEventsSentToStorage() {
		return EventsSentToStorage_;
	}

	static uint32_t getCurrentBurstId() {
		return burstToBeSet;
	}

	static uint NUMBER_OF_EBS;

	tbb::task* execute();

private:
<<<<<<< HEAD

	void handleL0Data(l0::MEPFragment * MEPFragment);
	void handleLKRData(cream::LKREvent * lkrEvent);

	void processL1(Event *event);
	void processL2(Event * event);

	Event* getNewEvent(uint32_t eventNumber);

=======
>>>>>>> BuildL2Task added
	/*
	 * @return <true> if any packet has been sent (time has passed)
	 */
	void sendL1RequestToCREAMS(Event * event);

	static void SendEOBBroadcast(uint32_t eventNumber,
			uint32_t finishedBurstID);

	zmq::socket_t* L0Socket_;
	zmq::socket_t* LKrSocket_;

	static std::vector<Event*> unusedEvents_;
	static std::vector<Event*> eventPool_;

	static std::atomic<uint64_t>* L2Triggers_;

	static std::atomic<uint64_t> BytesSentToStorage_;
	static std::atomic<uint64_t> EventsSentToStorage_;

	static boost::timer::cpu_timer EOBReceivedTime_;
};

}
/* namespace na62 */
#endif /* EVENTBUILDER_H_ */
