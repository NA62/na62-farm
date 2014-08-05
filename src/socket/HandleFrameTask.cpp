/*
 * HandleFrameTask.cpp
 *
 *  Created on: Jun 27, 2014
 *      Author: Jonas Kunze (kunze.jonas@gmail.com)
 */

#include "HandleFrameTask.h"

#include <eventBuilding/SourceIDManager.h>
#include <exceptions/UnknownCREAMSourceIDFound.h>
#include <exceptions/UnknownSourceIDFound.h>
#include <glog/logging.h>
#include <l0/MEP.h>
#include <l0/MEPFragment.h>
#include <LKr/LKREvent.h>
#include <LKr/LKRMEP.h>
#include <net/ethernet.h>
#include <net/if_arp.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <options/Options.h>
#include <socket/PFringHandler.h>
#include <structs/Network.h>
#include <algorithm>
#include <atomic>
#include <cstdbool>
#include <cstdint>
#include <iostream>
#include <vector>

#include "../eventBuilding/BuildL1Task.h"
#include "../eventBuilding/BuildL2Task.h"
#include "../options/MyOptions.h"
#include "PacketHandler.h"

namespace na62 {

HandleFrameTask::HandleFrameTask(DataContainer&& _container) :
		container(std::move(_container)) {

}

HandleFrameTask::~HandleFrameTask() {
}

void HandleFrameTask::processARPRequest(struct ARP_HDR* arp) {
	/*
	 * Look for ARP requests asking for my IP
	 */
	if (arp->targetIPAddr == PFringHandler::GetMyIP()) { // This is asking for me
		struct DataContainer responseArp = EthernetUtils::GenerateARPv4(
				PFringHandler::GetMyMac().data(), arp->sourceHardwAddr,
				PFringHandler::GetMyIP(), arp->sourceIPAddr,
				ARPOP_REPLY);
		// FIXME: Which thread should we use to send data?
		PFringHandler::AsyncSendFrame(std::move(responseArp));
	}
}

tbb::task* HandleFrameTask::execute() {
	/*
	 * TODO read options only once at startup
	 */
	const uint16_t L0_Port = Options::GetInt(OPTION_L0_RECEIVER_PORT);
	const uint16_t CREAM_Port = Options::GetInt(OPTION_CREAM_RECEIVER_PORT);

	const uint16_t EOB_BROADCAST_PORT = Options::GetInt(
	OPTION_EOB_BROADCAST_PORT);

	try {
		struct UDP_HDR* hdr = (struct UDP_HDR*) container.data;
		uint16_t etherType = ntohs(hdr->eth.ether_type);
		uint8_t ipProto = hdr->ip.protocol;
		uint16_t destPort = ntohs(hdr->udp.dest);

		/*
		 * Check if we received an ARP request
		 */
		if (etherType != ETHERTYPE_IP || ipProto != IPPROTO_UDP) {
			if (etherType == ETHERTYPE_ARP) {
				// this will delete the data
				processARPRequest((struct ARP_HDR*) container.data);
			} else {
				delete[] container.data;
				return nullptr;
			}
		}

		/*
		 * Check checksum errors
		 */
		if (!checkFrame(hdr, container.length)) {
			delete[] container.data;
			return nullptr;
		}

		const char * UDPPayload = container.data + sizeof(struct UDP_HDR);
		const uint16_t & dataLength = ntohs(hdr->udp.len)
				- sizeof(struct udphdr);

		/*
		 *  Now let's see what's insight the packet
		 */
		if (destPort == L0_Port) {

			/*
			 * L0 Data
			 * * Length is hdr->ip.tot_len-sizeof(struct udphdr) and not container.length because of ethernet padding bytes!
			 */
			l0::MEP* mep = new l0::MEP(UDPPayload, dataLength, container.data);

			PacketHandler::MEPsReceivedBySourceID_[mep->getSourceID()]++;
			PacketHandler::EventsReceivedBySourceID_[mep->getSourceID()] +=
					mep->getNumberOfEvents();
			PacketHandler::BytesReceivedBySourceID_[mep->getSourceID()] +=
					container.length;

			for (int i = mep->getNumberOfEvents() - 1; i >= 0; i--) {
				l0::MEPFragment* event = mep->getEvent(i);
				BuildL1Task* task =
						new (tbb::task::allocate_root()) BuildL1Task(event);
				tbb::task::enqueue(*task, tbb::priority_t::priority_low);
			}
		} else if (destPort == CREAM_Port) {
			/*
			 * CREAM Data
			 * Length is hdr->ip.tot_len-sizeof(struct iphdr) and not container.length because of ethernet padding bytes!
			 */
			cream::LKRMEP* mep = new cream::LKRMEP(UDPPayload, dataLength,
					container.data);

			PacketHandler::MEPsReceivedBySourceID_[SOURCE_ID_LKr]++;
			PacketHandler::EventsReceivedBySourceID_[SOURCE_ID_LKr] +=
					mep->getNumberOfEvents();
			PacketHandler::BytesReceivedBySourceID_[SOURCE_ID_LKr] +=
					container.length;

			/*
			 * Start builder tasks for every MEP fragment
			 */
			for (int i = mep->getNumberOfEvents() - 1; i >= 0; i--) {
				BuildL2Task* task =
						new (tbb::task::allocate_root()) BuildL2Task(
								mep->getEvent(i));
				tbb::task::enqueue(*task, tbb::priority_t::priority_low);
			}
		} else if (destPort == EOB_BROADCAST_PORT) {
			if (dataLength != sizeof(struct EOB_FULL_FRAME) - sizeof(UDP_HDR)) {
				LOG(ERROR)<<
				"Unrecognizable packet received at EOB farm broadcast Port!";
				delete[] container.data;
				return nullptr;
			}
			EOB_FULL_FRAME* pack = (struct EOB_FULL_FRAME*) container.data;
			LOG(INFO) <<
			"Received EOB Farm-Broadcast. Will increment BurstID now to " << pack->finishedBurstID + 1;
			BuildL1Task::setNextBurstID(pack->finishedBurstID + 1);
		} else {
			/*
			 * Packet with unknown UDP port received
			 */
			LOG(ERROR) <<"Packet with unknown UDP port received: " << destPort;
			delete[] container.data;
			return nullptr;
		}
	} catch (UnknownSourceIDFound const& e) {
		delete[] container.data;
	} catch (UnknownCREAMSourceIDFound const&e) {
		delete[] container.data;
	} catch (NA62Error const& e) {
		delete[] container.data;
	}
	return nullptr;
}

bool HandleFrameTask::checkFrame(struct UDP_HDR* hdr, uint16_t length) {
	/*
	 * Check IP-Header
	 */
	//				if (!EthernetUtils::CheckData((char*) &hdr->ip, sizeof(iphdr))) {
	//					LOG(ERROR) << "Packet with broken IP-checksum received");
	//					delete[] container.data;
	//					continue;
	//				}
	if (ntohs(hdr->ip.tot_len) + sizeof(ether_header) != length) {
		/*
		 * Does not need to be equal because of ethernet padding
		 */
		if (ntohs(hdr->ip.tot_len) + sizeof(ether_header) > length) {
			LOG(ERROR)<<
			"Received IP-Packet with less bytes than ip.tot_len field! " << (ntohs(hdr->ip.tot_len) + sizeof(ether_header) ) << ":"<<length;
			return false;
		}
	}

	/*
	 * Does not need to be equal because of ethernet padding
	 */
	if (ntohs(hdr->udp.len) + sizeof(ether_header) + sizeof(iphdr)
			> length) {
		LOG(ERROR)<<"Received UDP-Packet with less bytes than udp.len field! "<<(ntohs(hdr->udp.len) + sizeof(ether_header) + sizeof(iphdr)) <<":"<<length;
		return false;
	}

	//				/*
	//				 * Check UDP checksum
	//				 */
	//				if (!EthernetUtils::CheckUDP(hdr, (const char *) (&hdr->udp) + sizeof(struct udphdr), ntohs(hdr->udp.len) - sizeof(struct udphdr))) {
	//					LOG(ERROR) << "Packet with broken UDP-checksum received" );
	//					delete[] container.data;
	//					continue;
	//				}
	return true;
}

}
/* namespace na62 */
