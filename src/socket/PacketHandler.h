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
#include <iostream>
#include <utils/AExecutable.h>
#include <boost/timer/timer.hpp>
#include <eventBuilding/EventPool.h>
#include <eventBuilding/Event.h>
#include "../monitoring/FarmStatistics.h"

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
	static std::atomic<uint> packets_;
	static std::atomic<uint> sleeps_;
	static boost::timer::cpu_timer sendTimer;

	/*
	 * Number of times a HandleFrameTask object has been created and enqueued
	 */
	static std::atomic<uint> frameHandleTasksSpawned_;

private:
	int threadNum_;
	bool running_;
	static uint NUMBER_OF_EBS;
	std::string timeSource;
	/**
	 * @return <true> In case of success, false in case of a serious error (we should stop the thread in this case)
	 */
	void thread();
};

} /* namespace na62 */
#endif /* PACKETHANDLER_H_ */
