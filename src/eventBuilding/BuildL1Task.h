/*
 * HandleFrameTask.h
 *
 *  Created on: Jun 27, 2014
 *      Author: Jonas Kunze (kunze.jonas@gmail.com)
 */

#ifndef BUILDL1TASK_H_
#define BUILDL1TASK_H_

#include <tbb/task.h>

namespace na62 {
namespace l0 {
class MEPEvent;
} /* namespace l0 */
} /* namespace na62 */

namespace na62 {

class BuildL1Task: public tbb::task {
private:
	l0::MEPEvent* mepEvent_;
	static std::atomic<uint64_t>* L1Triggers_;

	uint32_t getCurrentBurstID() {
		// TODO: to be implemented
		return 0;
	}

	void setNextBurstID(uint32_t) {

	}

	void processL1(Event *event);

	static void sendEOBBroadcast(uint32_t eventNumber,
			uint32_t finishedBurstID);

	/*
	 * @return <true> if any packet has been sent (time has passed)
	 */
	void sendL1RequestToCREAMS(Event * event);

	static inline const std::atomic<uint64_t>* GetL1TriggerStats() {
		return L1Triggers_;
	}
public:
	BuildL1Task(l0::MEPEvent* event);
	virtual ~BuildL1Task();

	tbb::task* execute();
};

} /* namespace na62 */

#endif /* BUILDL1TASK_H_ */
