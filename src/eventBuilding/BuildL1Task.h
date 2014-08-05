/*
 * HandleFrameTask.h
 *
 *  Created on: Jun 27, 2014
 *      Author: Jonas Kunze (kunze.jonas@gmail.com)
 */

#ifndef BUILDL1TASK_H_
#define BUILDL1TASK_H_

#include <tbb/task.h>
#include <atomic>
#include <cstdint>

namespace na62 {
class Event;
namespace l0 {
class MEPFragment;
} /* namespace l0 */
} /* namespace na62 */

namespace na62 {

class BuildL1Task: public tbb::task {
private:
	l0::MEPFragment* MEPFragment_;
	static std::atomic<uint64_t>* L1Triggers_;
	static uint32_t currentBurstID_;

	uint32_t getCurrentBurstID() {
		// TODO: to be implemented
		return 0;
	}

	void processL1(Event *event);

	static void sendEOBBroadcast(uint32_t eventNumber,
			uint32_t finishedBurstID);

	/*
	 * @return <true> if any packet has been sent (time has passed)
	 */
	void sendL1RequestToCREAMS(Event * event);
public:
	BuildL1Task(l0::MEPFragment* event);
	virtual ~BuildL1Task();

	tbb::task* execute();

	static void setNextBurstID(uint32_t) {
		// TODO to be implemented
	}

	static uint32_t getCurrentBurstId() {
		return currentBurstID_;
	}

	static inline const std::atomic<uint64_t>* GetL1TriggerStats() {
		return L1Triggers_;
	}
};

} /* namespace na62 */

#endif /* BUILDL1TASK_H_ */
