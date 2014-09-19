/*
 * HandleFrameTask.h
 *
 *  Created on: Jun 27, 2014
 *      Author: Jonas Kunze (kunze.jonas@gmail.com)
 */

#ifndef L2BUILDER_H_
#define L2BUILDER_H_

#include <atomic>
#include <cstdint>

namespace na62 {
class Event;
namespace cream {
class LkrFragment;
} /* namespace cream */
} /* namespace na62 */

namespace na62 {

class L2Builder {
private:
	static std::atomic<uint64_t>* L2Triggers_;

	static std::atomic<uint64_t> BytesSentToStorage_;
	static std::atomic<uint64_t> EventsSentToStorage_;

	static uint32_t getCurrentBurstID() {
		return 0;
	}

	static void setNextBurstID(uint32_t) {
	}

public:
	static void buildEvent(cream::LkrFragment* lkrFragment);

	static void processL2(Event *event);

	static inline const std::atomic<uint64_t>* GetL2TriggerStats() {
		return L2Triggers_;
	}

	static inline const uint64_t GetBytesSentToStorage() {
		return BytesSentToStorage_;
	}

	static inline const uint64_t GetEventsSentToStorage() {
		return EventsSentToStorage_;
	}

	static void Initialize() {
		for (int i = 0; i != 0xFF + 1; i++) {
			L2Triggers_[i] = 0;
		}
	}
};

} /* namespace na62 */

#endif /* L2BUILDER_H_ */
