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

#include "../options/MyOptions.h"

namespace na62 {
class Event;
namespace l0 {
class MEPFragment;
} /* namespace l0 */
} /* namespace na62 */

namespace na62 {

class L1Builder {
private:
	static std::atomic<uint64_t>* L1Triggers_;

	static void processL1(Event *event);

	static bool requestZSuppressedLkrData_;

	static uint downscaleFactor_;

	static bool L1_flag_mode_;

	/*
	 * @return <true> if any packet has been sent (time has passed)
	 */
	static void sendL1RequestToCREAMS(Event * event);

public:
	/**
	 * Adds the fragment to the corresponding event and processes the L1 trigger
	 * algorithm if the event building is finished
	 *
	 * @ return true if the event is complete and therefore L1 has been processed, false otherwise
	 */
	static bool buildEvent(l0::MEPFragment* fragment, uint_fast32_t burstID);

	static inline std::atomic<uint64_t>* GetL1TriggerStats() {
		return L1Triggers_;
	}

	static void initialize() {
		for (int i = 0; i != 0xFF + 1; i++) {
			L1Triggers_[i] = 0;
		}

		requestZSuppressedLkrData_ = MyOptions::GetBool(OPTION_SEND_MRP_WITH_ZSUPPRESSION_FLAG);

		downscaleFactor_ = Options::GetInt(OPTION_L1_DOWNSCALE_FACTOR);

		L1_flag_mode_ = MyOptions::GetBool(OPTION_L1_FLAG_MODE);
	}
};

} /* namespace na62 */

#endif /* L1BUILDER_H_ */
