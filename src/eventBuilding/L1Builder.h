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

	static std::atomic<uint64_t> L1InputEvents_;
	static std::atomic<uint64_t> L1InputEventsPerBurst_;

	static std::atomic<uint64_t> L1AcceptedEvents_;

	static std::atomic<uint64_t> L1BypassedEvents_;

	static std::atomic<uint64_t> L1RequestToCreams_;

	static std::atomic<uint64_t> L0BuildingTimeCumulative_;
	static std::atomic<uint64_t> L0BuildingTimeMax_;
	static std::atomic<uint64_t> L1ProcessingTimeCumulative_;
	static std::atomic<uint64_t> L1ProcessingTimeMax_;
	static std::atomic<uint64_t>** L0BuildingTimeVsEvtNumber_;
	static std::atomic<uint64_t>** L1ProcessingTimeVsEvtNumber_;

	static void processL1(Event *event);

	static bool requestZSuppressedLkrData_;

	static uint reductionFactor_;

	static uint downscaleFactor_;

//	static bool L1_flag_mode_;
	static uint16_t l1FlagMask_;
	static uint autoFlagFactor_;

	static uint l1TriggerMask_;
	static uint l1ReferenceTimeSource_;
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

	static inline uint64_t GetL1InputStats() {
		return L1InputEvents_;
	}

	static inline uint64_t GetL1RequestToCreams() {
		return L1RequestToCreams_;
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

	static inline uint64_t GetL1InputEventsPerBurst() {
		return L1InputEventsPerBurst_;
	}

	static void ResetL1InputEventsPerBurst() {
		L1InputEventsPerBurst_ = 0;
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

	static inline uint64_t GetL1BypassedEvents() {
		return L1BypassedEvents_;
	}
	static inline uint GetL1DownscaleFactor() {
		return downscaleFactor_;
	}
	static inline uint GetL1ReductionFactor() {
		return reductionFactor_;
	}
//	static inline bool GetL1FlagMode() {
//		return L1_flag_mode_;
//	}
	static inline uint GetL1AutoFlagFactor() {
		return autoFlagFactor_;
	}
	static inline uint16_t GetL1FlagMask() {
		return l1FlagMask_;
	}
	static inline uint GetL1TriggerMask() {
		return l1TriggerMask_;
	}
	static inline uint GetL1ReferenceTimeSource() {
		return l1ReferenceTimeSource_;
	}
	static void initialize() {
		for (int i = 0; i != 0xFF + 1; i++) {
			L1Triggers_[i] = 0;
		}
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

		reductionFactor_ = Options::GetInt(OPTION_L1_REDUCTION_FACTOR);

		downscaleFactor_ = Options::GetInt(OPTION_L1_DOWNSCALE_FACTOR);

//		L1_flag_mode_ = MyOptions::GetBool(OPTION_L1_FLAG_MODE);
		l1FlagMask_ = MyOptions::GetInt(OPTION_L1_FLAG_MASK);

		autoFlagFactor_ = Options::GetInt(OPTION_L1_AUTOFLAG_FACTOR);
		l1ReferenceTimeSource_ = Options::GetInt(OPTION_L1_REFERENCE_TIME);

		std::stringstream hexToInt;
		hexToInt << std::hex << (MyOptions::GetString(OPTION_L1_TRIGGER_MASK));
		hexToInt >> l1TriggerMask_;
//		LOG_INFO << "l1TriggerMask " << std::hex << (uint)l1TriggerMask_ << ENDL;
	}
};

} /* namespace na62 */

#endif /* L1BUILDER_H_ */
