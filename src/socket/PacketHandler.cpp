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

#include <exceptions/UnknownSourceIDFound.h>
#include <l0/MEP.h>
#include <l0/MEPFragment.h>
#include <l1/L1DistributionHandler.h>
#include <l1/MEPFragment.h>
#include "../options/MyOptions.h"
#include <structs/Event.h>
#include <structs/Network.h>
#include <socket/EthernetUtils.h>
#include <socket/NetworkHandler.h>
#include <eventBuilding/SourceIDManager.h>
#include <boost/timer/timer.hpp>
#include <options/Logging.h>
#include <monitoring/BurstIdHandler.h>

#include "HandleFrameTask.h"
#include "TaskProcessor.h"

namespace na62 {

std::atomic<uint> PacketHandler::spins_;
std::atomic<uint> PacketHandler::sleeps_;

boost::timer::cpu_timer PacketHandler::sendTimer;


std::atomic<uint> PacketHandler::frameHandleTasksSpawned_(0);

PacketHandler::PacketHandler(int threadNum) :
		threadNum_(threadNum), running_(true) {}

PacketHandler::~PacketHandler() {
}

void PacketHandler::thread() {
	pfring_pkthdr hdr;
	memset(&hdr, 0, sizeof(hdr));
	int receivedFrame = 0;

	const bool activePolling = Options::GetBool(OPTION_ACTIVE_POLLING);
	const uint pollDelay = Options::GetDouble(OPTION_POLLING_DELAY);

	const uint maxAggregationMicros = Options::GetInt(
	OPTION_MAX_AGGREGATION_TIME);

	const uint minUsecBetweenL1Requests = Options::GetInt(
	OPTION_MIN_USEC_BETWEEN_L1_REQUESTS);

	uint sleepMicros = Options::GetInt(OPTION_POLLING_SLEEP_MICROS);

	const uint framesToBeGathered = Options::GetInt(OPTION_MAX_FRAME_AGGREGATION);

	//boost::timer::cpu_timer sendTimer;
	sleepMicros = Options::GetInt(OPTION_POLLING_SLEEP_MICROS);
	char* buff; // = new char[MTU];
	while (running_) {
		/*
		 * We want to aggregate several frames if we already have more HandleFrameTasks running than there are CPU cores available
		 */
		std::vector<DataContainer> frames;
		frames.reserve(framesToBeGathered);

		receivedFrame = 0;
		buff = nullptr;
		bool goToSleep = false;

		uint spinsInARow = 0;

		boost::timer::cpu_timer aggregationTimer;

		/*
		 * Try to receive [framesToBeCollected] frames
		 */
		for (uint stepNum = 0; stepNum != framesToBeGathered; stepNum++) {
			if (!running_) {
				goto finish;
			}
			/*
			 * The actual  polling!
			 * Do not wait for incoming packets as this will block the ring and make sending impossible
			 */
			receivedFrame = NetworkHandler::GetNextFrame(&hdr, &buff, 0, false,
					threadNum_);

			if (receivedFrame > 0) {

				/*
				 * Check if the burst should be flushed else prepare the data to be handled
				 */
				if(!BurstIdHandler::flushBurst()) {
					if (hdr.len > MTU) {
						LOG_ERROR("Received packet from network with size " << hdr.len << ". Dropping it");
					}
					else {
						char* data = new char[hdr.len];
						memcpy(data, buff, hdr.len);
						frames.push_back( { data, (uint_fast16_t) hdr.len, true });
						goToSleep = false;
						spinsInARow = 0;
					}
				}
				else {
					LOG_WARNING("Dropping data because we are at EoB");
				}
			}

				//GLM: probably we should remove the timer from here...
				//if(threadNum_ == 0 && NetworkHandler::getNumberOfEnqueuedSendFrames() > 0 ) {
				while (NetworkHandler::getNumberOfEnqueuedSendFrames() > 0 ) {
					//&& sendTimer.elapsed().wall / 1000 > minUsecBetweenL1Requests) {

					/*
					 * We didn't receive anything for a while -> send enqueued frames
					 */

					// GLM: keep this while loop else performance is a disaster!
					NetworkHandler::DoSendQueuedFrames(threadNum_);
//					sleepMicros =sleepMicros > minUsecBetweenL1Requests ?
//															minUsecBetweenL1Requests : sleepMicros;
//											spinsInARow = 0;
					//sendTimer.start();

					/*
					 * Push the aggregated frames to a new task if already tried to send something
					 * two times during current frame aggregation
					 */
				}
//				else {
//					if (!running_) {
//						goto finish;
//					}
//					//if (threadNum_ == 0 && NetworkHandler::getNumberOfEnqueuedSendFrames() != 0) {
//					//	continue;
//					//}
//
//					/*
//					 * If we didn't receive anything at the first try or in average for a while go to sleep
//					 */
//					if ((stepNum == 0 || spinsInARow++ == 10
//							|| aggregationTimer.elapsed().wall / 1000
//							> maxAggregationMicros)
//							&& (threadNum_ != 0
//									|| NetworkHandler::getNumberOfEnqueuedSendFrames() == 0)) {
//						goToSleep = true;
//						break;
//					}
//
//					/*
//					 * Spin wait a while. This block is not optimized by the compiler
//					 */
//					spins_++;
//					for (volatile uint i = 0; i < pollDelay; i++) {
//						asm("");
//					}
//
//				}

		}
		if (!frames.empty()) {

			/*
			 * Start a new task which will check the frame
			 *
			 */
			//HandleFrameTask* task =
			//		new (tbb::task::allocate_root()) HandleFrameTask(
			//				std::move(frames), BurstIdHandler::getCurrentBurstId());
			//tbb::task::enqueue(*task, tbb::priority_t::priority_normal);

			HandleFrameTask* task = new HandleFrameTask(std::move(frames), BurstIdHandler::getCurrentBurstId());
			TaskProcessor::TasksQueue_.push(task);
			int queueSize = TaskProcessor::getSize();
			if(queueSize >0 && (queueSize%100 == 0)) {
				LOG_WARNING("Tasks queue size " << (int) queueSize);
			}
			goToSleep = false;
			frameHandleTasksSpawned_++;
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

	finish: LOG_INFO("Stopping PacketHandler thread " << threadNum_);
}
}
/* namespace na62 */
