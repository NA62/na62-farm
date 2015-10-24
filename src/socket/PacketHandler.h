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
#include <socket/EthernetUtils.h>
#include <l0/MEP.h>

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

	bool checkFrame(UDP_HDR* hdr, uint_fast16_t length);
	void processARPRequest(ARP_HDR* arp);bool checkIfArp(
			uint_fast16_t etherType, DataContainer container,
			uint_fast8_t ipProto);
	void buildL1Event(l0::MEP* mep);
	void buildL2Event(l0::MEP* mep);
	void buildNSTDEvent(l0::MEP* mep);
	void processDestPortL0(const char* UDPPayload,
			uint_fast16_t UdpDataLength, DataContainer container);
	void processDestPortCREAM(const char* UDPPayload,
			uint_fast16_t UdpDataLength, DataContainer container);
	void processDestPortSTRAW(DataContainer container);

private:

	static uint_fast16_t L0_Port;
	static uint_fast16_t CREAM_Port;
	static uint_fast16_t STRAW_PORT;
	static uint_fast32_t MyIP;

	static uint highestSourceNum_;
	static std::atomic<uint64_t>* MEPsReceivedBySourceNum_;
	static std::atomic<uint64_t>* BytesReceivedBySourceNum_;

	int threadNum_;bool running_;
	static uint NUMBER_OF_EBS;

	/**
	 * @return <true> In case of success, false in case of a serious error (we should stop the thread in this case)
	 */
	void thread();

};

} /* namespace na62 */
#endif /* PACKETHANDLER_H_ */
