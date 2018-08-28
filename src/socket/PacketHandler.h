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
	static boost::timer::cpu_timer timer_;

	static void inline resetTimer() {
		timer_.stop();
		timer_.start();
	}
	std::array<std::atomic<uint64_t>, 301> RxPacketsVsTime_;
	std::array<std::atomic<uint64_t>, 301> TxPacketsVsTime_;

	std::array<std::atomic<uint64_t>, 301> const & getRxPacketsVsTime() const {
		return RxPacketsVsTime_;
	}
	std::array<std::atomic<uint64_t>, 301> const & getTxPacketsVsTime() const {
		return TxPacketsVsTime_;
	}

	void resetStats() {
			std::fill(RxPacketsVsTime_.begin(), RxPacketsVsTime_.end(), 0);
			std::fill(TxPacketsVsTime_.begin(), TxPacketsVsTime_.end(), 0);
	}

	u_int32_t getTime() const {
		//Returns the number of wall microseconds
		//return timer_.elapsed().wall / 1E3;
		//Returns the amount of 100ms (tenth of second)
		return timer_.elapsed().wall / 1E8;
	}

	/*
	 * Number of times a HandleFrameTask object has been created and enqueued
	 */
	static std::atomic<uint> frameHandleTasksSpawned_;

private:
	int threadNum_;
	bool running_;

	/**
	 * @return <true> In case of success, false in case of a serious error (we should stop the thread in this case)
	 */
	void thread();
};

} /* namespace na62 */
#endif /* PACKETHANDLER_H_ */
