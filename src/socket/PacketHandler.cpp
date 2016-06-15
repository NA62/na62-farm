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
	ssize_t receivedFrame = 0;

	const bool activePolling = Options::GetBool(OPTION_ACTIVE_POLLING);
	//const uint pollDelay = Options::GetDouble(OPTION_POLLING_DELAY);

	//const uint maxAggregationMicros = Options::GetInt(
	//OPTION_MAX_AGGREGATION_TIME);

	//const uint minUsecBetweenL1Requests = Options::GetInt(
	//OPTION_MIN_USEC_BETWEEN_L1_REQUESTS);

	uint sleepMicros = Options::GetInt(OPTION_POLLING_SLEEP_MICROS);

	const uint framesToBeGathered = Options::GetInt(OPTION_MAX_FRAME_AGGREGATION);

	std::string device = Options::GetString(OPTION_ETH_DEVICE_NAME);
	NetworkHandler::net_bind_udp(device);
	NetworkHandler::net_bind_udpl1(device);
	//boost::timer::cpu_timer sendTimer;

	char* buff; // = new char[MTU];
	in_port_t sourcePort = 0;
	in_addr_t sourceAddr = 0;
    //mmsghdr msg;
	while (running_) {

		/*
		 * We want to aggregate several frames if we already have more HandleFrameTasks running than there are CPU cores available
		 */
		std::vector<DataContainer> frames;
		frames.reserve(framesToBeGathered);

		receivedFrame = 0;
		buff = nullptr;
		bool goToSleep = false;

		//uint spinsInARow = 0;

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

			//receivedFrame = NetworkHandler::GetNextFrame(&hdr, &buff, 0, false, threadNum_);

			receivedFrame = NetworkHandler::GetNextFrame(&buff, sourcePort, sourceAddr, false, threadNum_);



			if (receivedFrame > 0) {

				/*
				 * Check if the burst should be flushed else prepare the data to be handled
				 */
				if(!BurstIdHandler::flushBurst()) {
					if (receivedFrame > MTU) {
						LOG_ERROR("Received packet from network with size " << receivedFrame << ". Dropping it");
					}
					else {
						char* data = new char[receivedFrame];
						memcpy(data, buff, receivedFrame);
						frames.push_back( { data, (uint_fast16_t) receivedFrame, true, sourcePort, sourceAddr });
						goToSleep = false;
						//spinsInARow = 0;
					}
				}
				//else {
				//	LOG_WARNING("Dropping data because we are at EoB");
				//}
			}
			//GLM: send all pending data requests
			while (NetworkHandler::getNumberOfEnqueuedSendFrames() > 0 ) {
				NetworkHandler::DoSendQueuedFrames(threadNum_);
			}

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
