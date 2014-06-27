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
#include <glog/logging.h>
#include <atomic>
#include <cstdint>
#include <iostream>
#include <vector>

#include <utils/AExecutable.h>
#include <eventBuilding/Event.h>

namespace na62 {
class Event;
class L1TriggerProcessor;
class L2TriggerProcessor;

namespace cream {
class LKREvent;
} /* namespace cream */

namespace l0 {
class MEPEvent;
} /* namespace l0 */
} /* namespace na62 */

namespace zmq {
class socket_t;
} /* namespace zmq */

namespace na62 {

class EventBuilder: public AExecutable {
public:
	EventBuilder();
	virtual ~EventBuilder();
	static void Initialize();

	static inline const std::atomic<uint64_t>* GetL1TriggerStats() {
		return L1Triggers_;
	}

	static inline const std::atomic<uint64_t>* GetL2TriggerStats() {
		return L2Triggers_;
	}

	static inline const uint64_t GetBytesSentToStorage() {
		return BytesSentToStorage_;
	}

	static inline const uint64_t GetEventsSentToStorage() {
		return EventsSentToStorage_;
	}

	static void SetNextBurstID(uint32_t nextBurstID) {
		currentBurstID_ = nextBurstID;

		EOBReceivedTime_.start();
		LOG(INFO)<<"Changing BurstID to " << nextBurstID;
		for (unsigned int i = 0; i < Instances_.size(); i++) {
			Instances_[i]->setNextBurstID(nextBurstID);
		}
	}

	static uint32_t getCurrentBurstId() {
		return currentBurstID_;
	}

	static uint NUMBER_OF_EBS;

private:
	void thread();

	void handleL0Data(l0::MEPEvent * mepEvent);
	void handleLKRData(cream::LKREvent * lkrEvent);

	void processL1(Event *event);
	void processL2(Event * event);

	Event* getNewEvent(uint32_t eventNumber);

	/*
	 * @return <true> if any packet has been sent (time has passed)
	 */
	void sendL1RequestToCREAMS(Event * event);

	static void SendEOBBroadcast(uint32_t eventNumber,
			uint32_t finishedBurstID);

	inline uint32_t getCurrentBurstID() {
		return threadCurrentBurstID_;
	}

	/*
	 * The burst ID will be changed as soon as the next L0 element is received. This way we can still receive CREAM data.
	 */
	inline void setNextBurstID(uint32_t nextBurstID) {
		changeBurstID_ = true;
	}

	zmq::socket_t* L0Socket_;
	zmq::socket_t* LKrSocket_;

	std::vector<Event*> unusedEvents_;
	std::vector<Event*> eventPool_;

	bool changeBurstID_;
	static uint32_t currentBurstID_;
	uint32_t threadCurrentBurstID_;

	L1TriggerProcessor* L1processor_;
	L2TriggerProcessor* L2processor_;

	static std::vector<EventBuilder*> Instances_;

	static std::atomic<uint64_t>* L1Triggers_;
	static std::atomic<uint64_t>* L2Triggers_;

	static std::atomic<uint64_t> BytesSentToStorage_;
	static std::atomic<uint64_t> EventsSentToStorage_;

	static boost::timer::cpu_timer EOBReceivedTime_;
};

}
/* namespace na62 */
#endif /* EVENTBUILDER_H_ */
