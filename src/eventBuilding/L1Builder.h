/*
 * HandleFrameTask.h
 *
 *  Created on: Jun 27, 2014
 *      Author: Jonas Kunze (kunze.jonas@gmail.com)
 */

#ifndef L1BUILDER_H_
#define L1BUILDER_H_

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

class L1Builder: public tbb::task {
private:
	static std::atomic<uint64_t>* L1Triggers_;
	static uint32_t currentBurstID_;

	static void processL1(Event *event);

	static void sendEOBBroadcast(uint32_t eventNumber,
			uint32_t finishedBurstID);

	/*
	 * @return <true> if any packet has been sent (time has passed)
	 */
	static void sendL1RequestToCREAMS(Event * event);

public:
	static void buildEvent(l0::MEPFragment* fragment, uint32_t burstID);


	static inline const std::atomic<uint64_t>* GetL1TriggerStats() {
		return L1Triggers_;
	}

	static void Initialize() {
		for (int i = 0; i != 0xFF + 1; i++) {
			L1Triggers_[i] = 0;
		}
	}
};

} /* namespace na62 */

#endif /* L1BUILDER_H_ */
