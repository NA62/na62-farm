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

#include <exceptions/UnknownCREAMSourceIDFound.h>
#include <exceptions/UnknownSourceIDFound.h>
#include <l0/MEP.h>
#include <l0/MEPFragment.h>
#include <LKr/L1DistributionHandler.h>
#include <LKr/LkrFragment.h>
#include <LKr/LKRMEP.h>
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

void PacketHandler::Initialize() {
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
	const u_char* data; // = new char[MTU];
	struct pfring_pkthdr hdr;
	memset(&hdr, 0, sizeof(hdr));
	register int result = 0;
	int sleepMicros = 1;

	const bool activePolling = Options::GetBool(OPTION_ACTIVE_POLLING);
	const uint ThreadNum = std::thread::hardware_concurrency();

	while (running_) {
		/*
		 * We want to aggregate several frames if we already have more HandleFrameTasks running than there are CPU cores available
		 */
		int framesToBeGathered = HandleFrameTask::getNumberOfQeuedFrames()
				- ThreadNum;
		if (framesToBeGathered < 0) {
			framesToBeGathered = 1;
		} else {
			if (framesToBeGathered > 1024) {
				framesToBeGathered = 1024;
			}
		}

		std::vector<DataContainer> frames;
		frames.reserve(framesToBeGathered);

		result = 0;
		data = NULL;

		/*
		 * Try to receive [framesToBeCollected] frames
		 */
		for (int i = 0; i != framesToBeGathered; i++) {
			/*
			 * The actual  polling!
			 * Do not wait for incoming packets as this will block the ring and make sending impossible
			 */
			result = NetworkHandler::GetNextFrame(&hdr, &data, 0, false,
					threadNum_);
			if (result > 0) {
				char* buff = new char[hdr.len];
				memcpy(buff, data, hdr.len);
				frames.push_back( { buff, (uint16_t) hdr.len, true });
			} else {
				/*
				 * Spin wait a while. This block is not optimized by the compiler
				 */
				for (volatile int i = 0; i < 1E5; i++) {
					asm("");
				}
			}
		}

		if (!frames.empty()) {
			/*
			 * Start a new task which will check the frame
			 *
			 */
//			DataContainer container = { buff, (uint16_t) hdr.len, true };
			HandleFrameTask* task =
					new (tbb::task::allocate_root()) HandleFrameTask(
							std::move(frames));
			tbb::task::enqueue(*task, tbb::priority_t::priority_normal);

			sleepMicros = 1;
		} else {
			/*
			 * Use the time to send some packets
			 */
			if (cream::L1DistributionHandler::DoSendMRP(threadNum_)
					|| NetworkHandler::DoSendQueuedFrames(threadNum_) != 0) {
				sleepMicros = 1;
				continue;
			}

			boost::this_thread::interruption_point();

			if (!activePolling || (sleepMicros < 10 && activePolling)) {
				/*
				 * Spin wait
				 */
				for (volatile int i = 0; i < sleepMicros * 1E4; i++) {
					asm("");
				}
			}

			if (sleepMicros < 10) {
				sleepMicros *= 2;
			} else {
				if (!activePolling) {
					/*
					 * Allow other threads to execute
					 */
					boost::this_thread::sleep(boost::posix_time::microsec(100));
				}
			}
		}
	}
	std::cout << "Stopping PacketHandler thread " << threadNum_ << std::endl;
}
}
/* namespace na62 */
