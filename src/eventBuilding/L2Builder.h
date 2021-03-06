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
	static std::atomic<uint64_t> L1BuildingTimeCumulative_;
	static std::atomic<uint64_t> L1BuildingTimeMax_;
	static std::atomic<uint64_t> L2ProcessingTimeCumulative_;
	static std::atomic<uint64_t> L2ProcessingTimeMax_;

	static std::atomic<uint64_t>** L1BuildingTimeVsEvtNumber_;
	static std::atomic<uint64_t>** L2ProcessingTimeVsEvtNumber_;
	static std::atomic<uint64_t>** SerializationTimeVsEvtNumber_;

public:
	/**
	 * Adds the fragment to the corresponding event and processes the L2 trigger
	 * algorithm if the L2 event building is finished
	 *
	 * @return true if the event is complete and therefore L2 has been processed, false otherwise
	 */
	static void buildEvent(l1::MEPFragment* Fragment);

	static void processL2(Event *event);

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


	static inline std::atomic<uint64_t>** GetSerializationTimeVsEvtNumber() {
		return SerializationTimeVsEvtNumber_;
	}

	static void ResetSerializationTimeVsEvtNumber() {
			for (int i = 0; i < 0x64 + 1; ++i) {
				for (int j = 0; j < 0x64 + 1; ++j) {
					SerializationTimeVsEvtNumber_[i][j] = 0;
				}
			}
	}

	static void initialize() {
		L1BuildingTimeVsEvtNumber_ = new std::atomic<uint64_t>*[0x64 + 1];
		L2ProcessingTimeVsEvtNumber_ = new std::atomic<uint64_t>*[0x64 + 1];
		SerializationTimeVsEvtNumber_ = new std::atomic<uint64_t>*[0x64 + 1];
		for (int i = 0; i < 0x64 + 1; i++) {
			L1BuildingTimeVsEvtNumber_[i] =
					new std::atomic<uint64_t>[0x64 + 1] { };
			L2ProcessingTimeVsEvtNumber_[i] =
					new std::atomic<uint64_t>[0x64 + 1] { };
			SerializationTimeVsEvtNumber_[i] =
					new std::atomic<uint64_t>[0x64 + 1] { };
		}
		L2Builder::ResetL1BuidingTimeVsEvtNumber();
		L2Builder::ResetL2ProcessingTimeVsEvtNumber();
		L2Builder::ResetSerializationTimeVsEvtNumber();
	}
};

} /* namespace na62 */

#endif /* L2BUILDER_H_ */
