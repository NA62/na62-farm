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
#include <options/Logging.h>
#include <monitoring/BurstIdHandler.h>
#include "../monitoring/FarmStatistics.h"

#include "HandleFrameTask.h"

#include "socket/PcapDumper.h"


namespace na62 {

std::atomic<uint> PacketHandler::spins_;
std::atomic<uint> PacketHandler::packets_;
std::atomic<uint> PacketHandler::sleeps_;

boost::timer::cpu_timer PacketHandler::sendTimer;

uint PacketHandler::NUMBER_OF_EBS = 0;
std::atomic<uint> PacketHandler::frameHandleTasksSpawned_(0);

PacketHandler::PacketHandler(int threadNum) :
		threadNum_(threadNum), running_(true) {
	NUMBER_OF_EBS = Options::GetInt(OPTION_NUMBER_OF_EBS);
	timeSource = FarmStatistics::getID(1);
}

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

	const uint framesToBeGathered = Options::GetInt(
	OPTION_MAX_FRAME_AGGREGATION);

	//boost::timer::cpu_timer sendTimer;
	LOG_INFO << "PacketHandler is running" << ENDL;
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
		LOG_INFO << "frames to be gathered " << framesToBeGathered << ENDL;
		for (uint stepNum = 0; stepNum != framesToBeGathered; stepNum++) {
			/*
			 * The actual  polling!
			 * Do not wait for incoming packets as this will block the ring and make sending impossible
			 */
//			LOG_INFO << "Begin of FOR" << ENDL;
			receivedFrame = NetworkHandler::GetNextFrame(&hdr, &buff, 0, false,
					threadNum_);
//			LOG_INFO << "Frames Got" << ENDL;
			if (receivedFrame > 0) {
				FarmStatistics::addTime("PH: " + timeSource + ", recieved frame ");
				char* data = new char[hdr.len];
				memcpy(data, buff, hdr.len);
				frames.push_back( { data, (uint_fast16_t) hdr.len, true });

				//LOG_ERROR<< "###########Packets" << ENDL;
				if(Options::GetBool(OPTION_DUMP_PACKETS)){
					//Dump of the packets
					PcapDumper::dumpPacket(threadNum_, data, hdr.len);
				}

				goToSleep = false;
				spinsInARow = 0;
//				LOG_INFO << "pushed recieved Frame" << ENDL;
			} else {
//				LOG_INFO << "recieved NO Frame" << ENDL;
				if (threadNum_ == 0
						&& sendTimer.elapsed().wall / 1000
								> minUsecBetweenL1Requests) {
//					LOG_INFO << "thread 0 waited enuff" << ENDL;
					/*
					 * We didn't receive anything for a while -> send enqueued frames
					 */
					sleepMicros = Options::GetInt(OPTION_POLLING_SLEEP_MICROS);

					if (NetworkHandler::DoSendQueuedFrames(threadNum_)) {
						sleepMicros =
								sleepMicros > minUsecBetweenL1Requests ?
										minUsecBetweenL1Requests : sleepMicros;
						spinsInARow = 0;
					}
					sendTimer.start();

					/*
					 * Push the aggregated frames to a new task if already tried to send something
					 * two times during current frame aggregation
					 */
				} else {
					if (!running_) {
						LOG_INFO << "finished" << ENDL;
						goto finish;
					}
//					if (threadNum_ == 0
//							&& NetworkHandler::getNumberOfEnqueuedSendFrames()
//									!= 0) {
//						continue;
//					}

					/*
					 * If we didn't receive anything at the first try or in average for a while go to sleep
					 */
					if ((stepNum == 0 || spinsInARow++ == 10
							|| aggregationTimer.elapsed().wall / 1000
									> maxAggregationMicros)
							&& (threadNum_ != 0
									|| NetworkHandler::getNumberOfEnqueuedSendFrames()
											== 0)) {
//						LOG_INFO << "I'm Sleeeeeeeeeping" << ENDL;
						goToSleep = true;
						break;
					}

					/*
					 * Spin wait a while. This block is not optimized by the compiler
					 */
					spins_++;
					for (volatile uint i = 0; i < pollDelay; i++) {
//						LOG_INFO << "\nSpin!\n" << ENDL;
						asm("");
					}
				}
			}
//			LOG_INFO << "New Frame gathered" << ENDL;
		}
//		LOG_INFO << "Out of FOR" << ENDL;
		if (!frames.empty()) {
			LOG_INFO << "PUSHING THE FRAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAMES!!!!!" << ENDL;
			/*
			 * Check if the burstID is already updated and the update is long enough ago. Otherwise
			 * we would increment the burstID while we are still processing events from the last burst.
			 */
			BurstIdHandler::checkBurstIdChange();

			/*
			 * Start a new task which will check the frame
			 *
			 */
			HandleFrameTask* task =
					new (tbb::task::allocate_root()) HandleFrameTask(
							std::move(frames),
							BurstIdHandler::getCurrentBurstId());
			tbb::task::enqueue(*task, tbb::priority_t::priority_normal);

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
	finish: LOG_INFO << "Stopping PacketHandler thread " << threadNum_ << ENDL;
}
}
/* namespace na62 */
