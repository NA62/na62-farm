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

namespace na62 {
struct DataContainer;

class PacketHandler: public AExecutable {
public:
	PacketHandler(int threadNum);
	virtual ~PacketHandler();

	void stopRunning() {
		running_ = false;
	}

	static void initialize();

	static std::atomic<uint> spins_;
	static std::atomic<uint> sleeps_;
	static boost::timer::cpu_timer sendTimer;

	/*
	 * Number of times a HandleFrameTask object has been created and enqueued
	 */
	static std::atomic<uint> frameHandleTasksSpawned_;

	static uint32_t getCurrentBurstId() {
		return currentBurstID_;
	}

	static uint32_t getNextBurstId() {
		return nextBurstID_;
	}

	static void setNextBurstId(uint32_t burstID) {
		nextBurstID_ = burstID;
		burstChangedTimer_.start();
	}

private:
	int threadNum_;bool running_;
	static uint NUMBER_OF_EBS;

	/*
	 * Store the current Burst ID and the next one separately. As soon as an EOB event is
	 * received the nextBurstID_ will be set. Then the currentBurstID will be updated later
	 * to make sure currently enqueued frames in other threads are not processed with
	 * the new burstID
	 */
	static uint32_t currentBurstID_;
	static uint32_t nextBurstID_;
	static boost::timer::cpu_timer burstChangedTimer_;

	/**
	 * @return <true> In case of success, false in case of a serious error (we should stop the thread in this case)
	 */
	void thread();
};

} /* namespace na62 */
#endif /* PACKETHANDLER_H_ */
