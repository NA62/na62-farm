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

	/**
	 * Method is called every time the last event of a burst has been processed
	 */
	static void onBurstFinished() {
		int maxNumOfPrintouts = 100;

		for (uint eventNumber = 0;
				eventNumber != EventPool::getLargestTouchedEventnumber() + 1;
				eventNumber++) {

			Event* event = EventPool::getEvent(eventNumber);
			if (event->isUnfinished()) {
				if (maxNumOfPrintouts-- == 0) {
					break;
				}

				std::cerr << "Unfinished event " << event->getEventNumber()
						<< ": " << std::endl;
				std::cerr << "\tMissing L0: " << std::endl;
				for (auto& sourceIDAndSubIds : event->getMissingSourceIDs()) {
					std::cerr << "\t"
							<< SourceIDManager::sourceIdToDetectorName(
									sourceIDAndSubIds.first) << ":"
							<< std::endl;

					for (auto& subID : sourceIDAndSubIds.second) {
						std::cerr << "\t\t" << subID << ", ";
					}
					std::cerr << std::endl;
				}
				std::cerr << std::endl;

				std::cerr << "\tMissing CREAMs (crate: cream IDs): " << std::endl;
				for (auto& crateAndCreams : event->getMissingCreams()) {
					std::cerr << "\t\t"<< crateAndCreams.first << ":\t";
					for (auto& creamID : crateAndCreams.second) {
						std::cerr << creamID << "\t";
					}
					std::cerr << std::endl;
				}
				std::cerr << std::endl;
			}
		}
	}

private:
	int threadNum_;
	bool running_;
	static uint NUMBER_OF_EBS;

	/**
	 * @return <true> In case of success, false in case of a serious error (we should stop the thread in this case)
	 */
	void thread();
};

} /* namespace na62 */
#endif /* PACKETHANDLER_H_ */
