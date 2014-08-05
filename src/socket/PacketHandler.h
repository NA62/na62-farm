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

namespace na62 {
struct DataContainer;

class PacketHandler: public AExecutable {
public:
	PacketHandler();
	virtual ~PacketHandler();

	static void Initialize();

	static inline uint64_t GetMEPsReceivedBySourceID(uint8_t sourceID) {
		return MEPsReceivedBySourceID_[sourceID];
	}

	static inline uint64_t GetEventsReceivedBySourceID(uint8_t sourceID) {
		return EventsReceivedBySourceID_[sourceID];
	}

	static inline uint64_t GetBytesReceivedBySourceID(uint8_t sourceID) {
		return BytesReceivedBySourceID_[sourceID];
	}

	static std::atomic<uint64_t>* MEPsReceivedBySourceID_;
	static std::atomic<uint64_t>* EventsReceivedBySourceID_;
	static std::atomic<uint64_t>* BytesReceivedBySourceID_;

private:
	/**
	 * @return <true> In case of success, false in case of a serious error (we should stop the thread in this case)
	 */
	void thread();
};

} /* namespace na62 */
#endif /* PACKETHANDLER_H_ */
