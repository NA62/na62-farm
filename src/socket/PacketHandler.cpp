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
#include "../options/MyOptions.h"
#include <structs/Event.h>
#include <structs/Network.h>
#include <socket/EthernetUtils.h>
#include <socket/NetworkHandler.h>
#include <eventBuilding/SourceIDManager.h>
#include <boost/timer/timer.hpp>

#include "HandleFrameTask.h"

namespace na62 {

std::atomic<uint> PacketHandler::spins_;
std::atomic<uint> PacketHandler::sleeps_;

boost::timer::cpu_timer PacketHandler::sendTimer;

uint PacketHandler::NUMBER_OF_EBS = 0;

PacketHandler::PacketHandler(int threadNum) :
		threadNum_(threadNum), running_(true) {
	NUMBER_OF_EBS = Options::GetInt(OPTION_NUMBER_OF_EBS);
}

PacketHandler::~PacketHandler() {
}

void PacketHandler::initialize() {
}

void PacketHandler::thread() {
	const u_char* data; // = new char[MTU];
	struct pfring_pkthdr hdr;
	memset(&hdr, 0, sizeof(hdr));
	int receivedFrame = 0;

	const bool activePolling = Options::GetBool(OPTION_ACTIVE_POLLING);
	const uint pollDelay = Options::GetFloat(OPTION_POLLING_DELAY);

	const uint minUsecBetweenL1Requests = Options::GetInt(
	OPTION_MIN_USEC_BETWEEN_L1_REQUESTS);

	uint sleepMicros = Options::GetInt(OPTION_POLLING_SLEEP_MICROS);

	const uint framesToBeGathered = Options::GetInt(
	OPTION_MAX_FRAME_AGGREGATION);

	//boost::timer::cpu_timer sendTimer;

	boost::timer::cpu_timer timeWithoutReceiving;
	uint maxUsecsWithoutReceiving = sleepMicros / 100;
	if (maxUsecsWithoutReceiving < 10) {
		maxUsecsWithoutReceiving = 10;
	}

	while (running_) {
		/*
		 * We want to aggregate several frames if we already have more HandleFrameTasks running than there are CPU cores available
		 */
		std::vector<DataContainer> frames;
		frames.reserve(framesToBeGathered);

		receivedFrame = 0;
		data = nullptr;
		bool goToSleep = false;

		/*
		 * Try to receive [framesToBeCollected] frames
		 */
		for (uint i = 0; i != framesToBeGathered; i++) {
			/*
			 * The actual  polling!
			 * Do not wait for incoming packets as this will block the ring and make sending impossible
			 */
			receivedFrame = NetworkHandler::GetNextFrame(&hdr, &data, 0, false,
					threadNum_);

			if (receivedFrame > 0) {
				char* buff = new char[hdr.len];
				memcpy(buff, data, hdr.len);
				frames.push_back( { buff, (uint16_t) hdr.len, true });
				goToSleep = false;
				timeWithoutReceiving.start();
			}

			if (threadNum_ == 0
					&& sendTimer.elapsed().wall / 1000
							> minUsecBetweenL1Requests) {
				/*
				 * We didn't receive anything for a while -> send enqueued frames
				 */
				sleepMicros = Options::GetInt(OPTION_POLLING_SLEEP_MICROS);
				if (NetworkHandler::DoSendQueuedFrames(threadNum_)) {
					sleepMicros =
							sleepMicros > minUsecBetweenL1Requests ?
									minUsecBetweenL1Requests : sleepMicros;
				}
				sendTimer.start();

				/*
				 * Push the aggregated frames to a new task if already tried to send something
				 * two times during current frame aggregation
				 */
			} else if (receivedFrame == 0) {
				/*
				 * If we didn't receive anything for a while go to sleep
				 */
				if (timeWithoutReceiving.elapsed().wall / 1000
						> maxUsecsWithoutReceiving) {
					goToSleep = true;
					break;
				}

				/*
				 * If we didn't receive anything at the first try go back to sleep
				 */
				if (i == 0) {
					goToSleep = true;
					break;
				}

				/*
				 * Spin wait a while. This block is not optimized by the compiler
				 */
				spins_++;
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

			goToSleep = false;
		} else {
			goToSleep = true;
		}

		if (goToSleep) {
			sleeps_++;
			if (!activePolling) {
				/*
				 * Allow other threads to run
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
