/*
 * HandleFrameTask.h
 *
 *  Created on: Jun 27, 2014
 *      Author: Jonas Kunze (kunze.jonas@gmail.com)
 */

#ifndef BUILDL2TASK_H_
#define BUILDL2TASK_H_

#include <tbb/task.h>
#include <atomic>
#include <cstdint>

namespace na62 {
class Event;
namespace cream {
class LKREvent;
} /* namespace cream */
} /* namespace na62 */

namespace na62 {
namespace l0 {
class MEPEvent;
} /* namespace l0 */
} /* namespace na62 */

namespace na62 {

class BuildL2Task: public tbb::task {
private:
	cream::LKREvent* lkrEvent_;

	static std::atomic<uint64_t>* L2Triggers_;

	std::atomic<uint64_t> BytesSentToStorage_;
	std::atomic<uint64_t> EventsSentToStorage_;

	uint32_t getCurrentBurstID() {
		return 0;
	}

	void setNextBurstID(uint32_t) {
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
public:
	BuildL2Task(l0::MEPEvent* event);
	virtual ~BuildL2Task();

	tbb::task* execute();
	static void processL2(Event *event);
};

} /* namespace na62 */

#endif /* BUILDL2TASK_H_ */
