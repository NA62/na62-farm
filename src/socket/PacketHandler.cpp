/*
 * PacketHandler.cpp
 *
 *  Created on: Feb 7, 2012
 *      Author: Jonas Kunze (kunze.jonas@gmail.com)
 */

#include "PacketHandler.h"

#include <asm-generic/errno-base.h>
#include <boost/date_time/posix_time/posix_time_duration.hpp>
#include <boost/date_time/time_duration.hpp>
#include <boost/thread/pthread/thread_data.hpp>
#include <glog/logging.h>
#include <linux/pf_ring.h>
#include <net/ethernet.h>
#include <net/if_arp.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <sys/types.h>
#include <zmq.h>
#include <zmq.hpp>
#include <algorithm>
#include <cstdbool>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <queue>

#include <exceptions/UnknownCREAMSourceIDFound.h>
#include <exceptions/UnknownSourceIDFound.h>
#include <l0/MEP.h>
#include <l0/MEPEvent.h>
#include <LKr/L1DistributionHandler.h>
#include <LKr/LKREvent.h>
#include <LKr/LKRMEP.h>
#include "../options/MyOptions.h"
#include <structs/Event.h>
#include <structs/Network.h>
#include <socket/EthernetUtils.h>
#include <socket/PFringHandler.h>

#include "../socket/ZMQHandler.h"
#include "../eventBuilding/EventBuilder.h"

namespace na62 {

uint NUMBER_OF_EBS = 0;

std::atomic<uint64_t>* PacketHandler::MEPsReceivedBySourceID_;
std::atomic<uint64_t>* PacketHandler::EventsReceivedBySourceID_;
std::atomic<uint64_t>* PacketHandler::BytesReceivedBySourceID_;

PacketHandler::PacketHandler() {
	NUMBER_OF_EBS = Options::GetInt(OPTION_NUMBER_OF_EBS);
}

PacketHandler::~PacketHandler() {
}

void PacketHandler::Initialize() {
	int highestSourceID = SourceIDManager::LARGEST_L0_DATA_SOURCE_ID;
	if (highestSourceID < SOURCE_ID_LKr) { // Add LKr
		highestSourceID = SOURCE_ID_LKr;
	}

	MEPsReceivedBySourceID_ = new std::atomic<uint64_t>[highestSourceID + 1];
	EventsReceivedBySourceID_ = new std::atomic<uint64_t>[highestSourceID + 1];
	BytesReceivedBySourceID_ = new std::atomic<uint64_t>[highestSourceID + 1];

	for (int i = 0; i <= highestSourceID; i++) {
		MEPsReceivedBySourceID_[i] = 0;
		EventsReceivedBySourceID_[i] = 0;
		BytesReceivedBySourceID_[i] = 0;
	}
}

void PacketHandler::thread() {
	connectZMQ();

	DataContainer container;

	std::queue<DataContainer> dataContainers;
	register char* data; // = new char[MTU];
	struct pfring_pkthdr hdr;
	memset(&hdr, 0, sizeof(hdr));
	register int result = 0;
	int sleepMicros = 1;
	while (true) {
		boost::this_thread::interruption_point();
		result = 0;
		data = NULL;
		/*
		 * The actual  polling!
		 * Do not wait for incoming packets as this will block the ring and make sending impossible
		 */
		result = PFringHandler::GetNextFrame(&hdr, &data, 0, false, threadNum_);
		if (result == 1) {
			char* buff = new char[hdr.len];
			memcpy(buff, data, hdr.len);
			container = {buff, (uint16_t) hdr.len};
			dataContainers.push(std::move(container));
			sleepMicros = 1;
		} else if (dataContainers.empty()) {
			/*
			 * Use the time to send some packets
			 */
			if (cream::L1DistributionHandler::DoSendMRP(threadNum_)) {
				sleepMicros = 1;
				continue;
			}
			boost::this_thread::sleep(boost::posix_time::microsec(sleepMicros));
			if (sleepMicros < 10000) {
				sleepMicros *= 2;
			}
		} else {
			if (!processPacket(dataContainers.front())) {
				dataContainers.pop();
				return; // stop running
			}
			dataContainers.pop();
		}
	}
}
}
/* namespace na62 */
