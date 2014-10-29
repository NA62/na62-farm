/*
 * PacketHandler.cpp
 *
 *  Created on: Feb 7, 2012
 *      Author: Jonas Kunze (kunze.jonas@gmail.com)
 */

#include "PacketHandler.h"

#include <tbb/task.h>
#include <tbb/tick_count.h>
#include <tbb/tbb_thread.h>
#ifdef USE_GLOG
#include <glog/logging.h>
#endif
#include <linux/pf_ring.h>
#include <net/ethernet.h>
#include <net/if_arp.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <sys/types.h>
#include <algorithm>
#include <cstdbool>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <queue>
#include <thread>
#include <tbb/tbb.h>

#include <exceptions/UnknownCREAMSourceIDFound.h>
#include <exceptions/UnknownSourceIDFound.h>
#include <l0/MEP.h>
#include <l0/MEPFragment.h>
#include <LKr/L1DistributionHandler.h>
#include <LKr/LkrFragment.h>
#include "../options/MyOptions.h"
#include <structs/Event.h>
#include <structs/Network.h>
#include <socket/EthernetUtils.h>
#include <socket/NetworkHandler.h>
#include <eventBuilding/SourceIDManager.h>

#include "HandleFrameTask.h"

namespace na62 {

uint NUMBER_OF_EBS = 0;

std::atomic<uint64_t>* PacketHandler::MEPsReceivedBySourceID_;
std::atomic<uint64_t>* PacketHandler::EventsReceivedBySourceID_;
std::atomic<uint64_t>* PacketHandler::BytesReceivedBySourceID_;

PacketHandler::PacketHandler(int threadNum) :
		threadNum_(threadNum), running_(true) {
	NUMBER_OF_EBS = Options::GetInt(OPTION_NUMBER_OF_EBS);
}

PacketHandler::~PacketHandler() {
}

void PacketHandler::initialize() {
	int highestSourceID = SourceIDManager::LARGEST_L0_DATA_SOURCE_ID;
	if (highestSourceID < SOURCE_ID_LKr) { // Add LKr
		highestSourceID = SOURCE_ID_LKr;
	}
	MEPsReceivedBySourceID_ = new std::atomic<uint64_t>[highestSourceID + 1];
	EventsReceivedBySourceID_ = new std::atomic<uint64_t>[highestSourceID + 1];
	BytesReceivedBySourceID_ = new std::atomic<uint64_t>[highestSourceID + 1];

	for (int i = 0; i <= highestSourceID; i++) {
		MEPsReceivedBySourceID_[i] = 0;
		EventsReceivedBySourceID_[i] = 0;
		BytesReceivedBySourceID_[i] = 0;
	}
}

void PacketHandler::thread() {
	struct pfring_pkthdr hdr;
	memset(&hdr, 0, sizeof(hdr));
	register int result = 0;
	int sleepMicros = 1;

	const bool activePolling = Options::GetBool(OPTION_ACTIVE_POLLING);
	const uint pollDelay = Options::GetFloat(OPTION_POLLING_DELAY);

	const uint framesToBeGathered = Options::GetInt(
	OPTION_MAX_FRAME_AGGREGATION);

	const uint64_t mainBufferSize = 10E9;
	u_char* mainBuffer = new u_char[mainBufferSize];
	memset(mainBuffer, 0, mainBufferSize);

	uint64_t mainBuffer_ptr = 0;
	while (running_) {
		/*
		 * We want to aggregate several frames if we already have more HandleFrameTasks running than there are CPU cores available
		 */
		std::vector<DataContainer> frames;
		frames.reserve(framesToBeGathered);

		result = 0;
		/*
		 * Try to receive [framesToBeCollected] frames
		 */
		for (uint i = 0; i != framesToBeGathered; i++) {
			/*
			 * The actual  polling!
			 * Do not wait for incoming packets as this will block the ring and make sending impossible
			 */
			const u_char* nextFrameBuffer = mainBuffer + mainBuffer_ptr;
			result = NetworkHandler::GetNextFrame(&hdr, &nextFrameBuffer, MTU,
			false, threadNum_);

			if (result > 0) {
				frames.push_back( { (char*) nextFrameBuffer, (uint16_t) hdr.len,
				false });
				mainBuffer_ptr = (mainBuffer_ptr + hdr.len) % mainBufferSize;
			} else {
				/*
				 * Spin wait a while. This block is not optimized by the compiler
				 */
				for (volatile uint i = 0; i < pollDelay; i++) {
					asm("");
				}
			}
		}

		if (!frames.empty()) {
			/*
			 * Start a new task which will check the frame
			 *
			 */
			HandleFrameTask* task =
					new (tbb::task::allocate_root()) HandleFrameTask(
							std::move(frames));
			tbb::task::enqueue(*task, tbb::priority_t::priority_normal);

			sleepMicros = 1;

			if (frames.size() != framesToBeGathered) {
				if (!cream::L1DistributionHandler::DoSendMRP(threadNum_)) {
					NetworkHandler::DoSendQueuedFrames(threadNum_);
				}
			}
		} else {
			/*
			 * Use the time to send some packets
			 */
			if (cream::L1DistributionHandler::DoSendMRP(threadNum_)
					|| NetworkHandler::DoSendQueuedFrames(threadNum_) != 0) {
				sleepMicros = 1;
				continue;
			}

			if (sleepMicros < 64) {
				sleepMicros *= 2;
			}

//				boost::this_thread::interruption_point();
			if (!activePolling) {
				/*
				 * Allow other threads to execute
				 */
				boost::this_thread::sleep(
						boost::posix_time::microsec(sleepMicros));
			}
		}
	}
	std::cout << "Stopping PacketHandler thread " << threadNum_ << std::endl;
}
}
/* namespace na62 */
