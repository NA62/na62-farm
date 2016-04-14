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

#include "../options/MyOptions.h"
namespace na62 {
class Event;
namespace l1 {
class MEPFragment;
} /* namespace cream */
} /* namespace na62 */

namespace na62 {

class L2Builder {
private:

//	static std::atomic<uint64_t>* L2Triggers_;
//	static std::atomic<uint64_t> L2InputEvents_;
//	static std::atomic<uint64_t> L2InputEventsPerBurst_;
//	static std::atomic<uint64_t> L2AcceptedEvents_;

	static std::atomic<uint64_t> L1BuildingTimeCumulative_;
	static std::atomic<uint64_t> L1BuildingTimeMax_;
	static std::atomic<uint64_t> L2ProcessingTimeCumulative_;
	static std::atomic<uint64_t> L2ProcessingTimeMax_;
	static std::atomic<uint64_t> BytesSentToStorage_;

	static std::atomic<uint64_t> EventsSentToStorage_;

	static std::atomic<uint64_t>** L1BuildingTimeVsEvtNumber_;
	static std::atomic<uint64_t>** L2ProcessingTimeVsEvtNumber_;
	//	static uint reductionFactor_;
	//	static uint downscaleFactor_;
public:
	/**
	 * Adds the fragment to the corresponding event and processes the L2 trigger
	 * algorithm if the L2 event building is finished
	 *
	 * @return true if the event is complete and therefore L2 has been processed, false otherwise
	 */
	static bool buildEvent(l1::MEPFragment* Fragment);

	static void processL2(Event *event);

//	static inline std::atomic<uint64_t>* GetL2TriggerStats() {
//		return L2Triggers_;
//	}

//	static inline uint64_t GetL2InputStats() {
//		return L2InputEvents_;
//	}

	static inline std::atomic<uint64_t>** GetL1BuidingTimeVsEvtNumber() {
		return L1BuildingTimeVsEvtNumber_;
	}

	static void ResetL1BuidingTimeVsEvtNumber() {
			for (int i = 0; i < 0x64 + 1; ++i) {
				for (int j = 0; j < 0x64 + 1; ++j) {
						L1BuildingTimeVsEvtNumber_[i][j] = 0;
				}
			}
	}

	static inline std::atomic<uint64_t>** GetL2ProcessingTimeVsEvtNumber() {
		return L2ProcessingTimeVsEvtNumber_;
	}

	static void ResetL2ProcessingTimeVsEvtNumber() {
			for (int i = 0; i < 0x64 + 1; ++i) {
				for (int j = 0; j < 0x64 + 1; ++j) {
						L2ProcessingTimeVsEvtNumber_[i][j] = 0;
				}
			}
	}

	static inline uint64_t GetL2ProcessingTimeCumulative() {
		return L2ProcessingTimeCumulative_;
	}

	static void ResetL2ProcessingTimeCumulative() {
		L2ProcessingTimeCumulative_ = 0;
	}

	static inline uint64_t GetL1BuildingTimeCumulative() {
		return L1BuildingTimeCumulative_;
	}

	static void ResetL1BuildingTimeCumulative() {
		L1BuildingTimeCumulative_ = 0;
	}

//	static inline uint64_t GetL2InputEventsPerBurst() {
//		return L2InputEventsPerBurst_;
//	}

//	static void ResetL2InputEventsPerBurst() {
//		L2InputEventsPerBurst_ = 0;
//	}

	static inline uint64_t GetL2ProcessingTimeMax() {
		return L2ProcessingTimeMax_;
	}

	static void ResetL2ProcessingTimeMax() {
		L2ProcessingTimeMax_ = 0;
	}

	static inline uint64_t GetL1BuildingTimeMax() {
		return L1BuildingTimeMax_;
	}

	static void ResetL1BuildingTimeMax() {
		L1BuildingTimeMax_ = 0;
	}

	static inline uint64_t GetBytesSentToStorage() {
		return BytesSentToStorage_;
	}

	static inline uint64_t GetEventsSentToStorage() {
		return EventsSentToStorage_;
	}

//	static inline uint64_t GetL2DownscaleFactor() {
//		return downscaleFactor_;
//	}
//	static inline uint64_t GetL2ReductionFactor() {
//		return reductionFactor_;
//	}

	static void initialize() {
//	for (int i = 0; i != 0xFF + 1; i++) {
//			L2Triggers_[i] = 0;
//		}
		L1BuildingTimeVsEvtNumber_ = new std::atomic<uint64_t>*[0x64 + 1];
		L2ProcessingTimeVsEvtNumber_ = new std::atomic<uint64_t>*[0x64 + 1];
		for (int i = 0; i < 0x64 + 1; i++) {
			L1BuildingTimeVsEvtNumber_[i] =
					new std::atomic<uint64_t>[0x64 + 1] { };
			L2ProcessingTimeVsEvtNumber_[i] =
					new std::atomic<uint64_t>[0x64 + 1] { };
		}
		L2Builder::ResetL1BuidingTimeVsEvtNumber();
		L2Builder::ResetL2ProcessingTimeVsEvtNumber();
		//	reductionFactor_ = Options::GetInt(OPTION_L2_REDUCTION_FACTOR);

		//	downscaleFactor_ = Options::GetInt(OPTION_L2_DOWNSCALE_FACTOR);
	}
};

} /* namespace na62 */

#endif /* L2BUILDER_H_ */
