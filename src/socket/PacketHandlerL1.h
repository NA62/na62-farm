/*
 * PacketHandlerL1.h
 *
 *  Created on: Jun 23, 2016
 *      Author: Julio Calvo
 */
#pragma once
#ifndef SOCKET_PACKETHANDLERL1_H_
#define SOCKET_PACKETHANDLERL1_H_

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

class PacketHandlerL1: public AExecutable {
public:
	PacketHandlerL1(int threadNum);
	virtual ~PacketHandlerL1();

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

private:
	int threadNum_;
	bool running_;

	/**
	 * @return <true> In case of success, false in case of a serious error (we should stop the thread in this case)
	 */
	void thread();
};

} /* namespace na62 */

#endif /* SOCKET_PACKETHANDLERL1_H_ */
