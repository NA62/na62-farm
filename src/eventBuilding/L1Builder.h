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
	static std::atomic<uint64_t> L1Requests_;

	static std::atomic<uint64_t> L0BuildingTimeCumulative_;
	static std::atomic<uint64_t> L0BuildingTimeMax_;
	static std::atomic<uint64_t> L1ProcessingTimeCumulative_;
	static std::atomic<uint64_t> L1ProcessingTimeMax_;
	static std::atomic<uint64_t>** L0BuildingTimeVsEvtNumber_;
	static std::atomic<uint64_t>** L1ProcessingTimeVsEvtNumber_;

	static void processL1(Event *event);

	static bool requestZSuppressedLkrData_;



public:

	/*
	 * @return <true> if any packet has been sent (time has passed)
	 */
	static void sendL1Request(Event * event);

	/**
	 * Adds the fragment to the corresponding event and processes the L1 trigger
	 * algorithm if the event building is finished
	 *
	 * @ return true if the event is complete and therefore L1 has been processed, false otherwise
	 */
	static void buildEvent(l0::MEPFragment* fragment, uint_fast32_t burstID);

	static inline std::atomic<uint64_t>** GetL0BuidingTimeVsEvtNumber() {
		return L0BuildingTimeVsEvtNumber_;
	}

	static void ResetL0BuidingTimeVsEvtNumber() {
		for (int i = 0; i < 0x64 + 1; ++i) {
			for (int j = 0; j < 0x64 + 1; ++j) {
				L0BuildingTimeVsEvtNumber_[i][j] = 0;
			}
		}
	}

	static inline std::atomic<uint64_t>** GetL1ProcessingTimeVsEvtNumber() {
		return L1ProcessingTimeVsEvtNumber_;
	}

	static void ResetL1ProcessingTimeVsEvtNumber() {
		for (int i = 0; i < 0x64 + 1; ++i) {
			for (int j = 0; j < 0x64 + 1; ++j) {
				L1ProcessingTimeVsEvtNumber_[i][j] = 0;
			}
		}
	}

	static inline uint64_t GetL1Requests() {
		return L1Requests_;
	}

	static inline uint64_t GetL1ProcessingTimeCumulative() {
		return L1ProcessingTimeCumulative_;
	}

	static void ResetL1ProcessingTimeCumulative() {
		L1ProcessingTimeCumulative_ = 0;
	}

	static inline uint64_t GetL0BuildingTimeCumulative() {
		return L0BuildingTimeCumulative_;
	}

	static void ResetL0BuildingTimeCumulative() {
		L0BuildingTimeCumulative_ = 0;
	}

	static inline uint64_t GetL1ProcessingTimeMax() {
		return L1ProcessingTimeMax_;
	}

	static void ResetL1ProcessingTimeMax() {
		L1ProcessingTimeMax_ = 0;
	}

	static inline uint64_t GetL0BuildingTimeMax() {
		return L0BuildingTimeMax_;
	}

	static void ResetL0BuildingTimeMax() {
		L0BuildingTimeMax_ = 0;
	}

	static void initialize() {
		L0BuildingTimeVsEvtNumber_ = new std::atomic<uint64_t>*[0x64 + 1];
		L1ProcessingTimeVsEvtNumber_ = new std::atomic<uint64_t>*[0x64 + 1];
		for (int i = 0; i < 0x64 + 1; i++) {
			L0BuildingTimeVsEvtNumber_[i] =
					new std::atomic<uint64_t>[0x64 + 1] { };
			L1ProcessingTimeVsEvtNumber_[i] =
					new std::atomic<uint64_t>[0x64 + 1] { };
		}
		L1Builder::ResetL0BuidingTimeVsEvtNumber();
		L1Builder::ResetL1ProcessingTimeVsEvtNumber();

		requestZSuppressedLkrData_ = MyOptions::GetBool(
		OPTION_SEND_MRP_WITH_ZSUPPRESSION_FLAG);
	}
};

} /* namespace na62 */

#endif /* L1BUILDER_H_ */
