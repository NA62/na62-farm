/*
 * PacketHandler.h
 *
 *  Created on: Feb 7, 2012
 *      Author: Jonas Kunze (kunze.jonas@gmail.com)
 */

#pragma once
#ifndef PACKETHANDLER_H_
#define PACKETHANDLER_H_

#include <sys/types.h>
#include <atomic>
#include <cstdint>
#include <vector>
#include <utils/AExecutable.h>
#include <boost/timer/timer.hpp>
#include <eventBuilding/EventPool.h>
#include <eventBuilding/Event.h>

namespace na62 {
struct DataContainer;

class PacketHandler: public AExecutable {
public:
	PacketHandler(int threadNum);
	virtual ~PacketHandler();

	void stopRunning() {
		running_ = false;
	}

	static std::atomic<uint> spins_;
	static std::atomic<uint> sleeps_;
	static boost::timer::cpu_timer sendTimer;

	/*
	 * Number of times a HandleFrameTask object has been created and enqueued
	 */
	static std::atomic<uint> frameHandleTasksSpawned_;

//	static void onBurstFinished() {
//		for (uint eventNumber = 0;
//				eventNumber != EventPool::getLargestTouchedEventnumber();
//				eventNumber++) {
//			Event* event = EventPool::getEvent(eventNumber);
//			if (event->isL1Processed()) {
//				if(!event->isL2Accepted()){
//					std::cerr << event->getEventNumber() << std::endl;
//				}
//			}
//		}
//	}

private:
	int threadNum_;bool running_;
	static uint NUMBER_OF_EBS;

	/**
	 * @return <true> In case of success, false in case of a serious error (we should stop the thread in this case)
	 */
	void thread();
};

} /* namespace na62 */
#endif /* PACKETHANDLER_H_ */
