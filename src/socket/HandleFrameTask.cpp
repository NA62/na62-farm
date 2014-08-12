/*
 * HandleFrameTask.cpp
 *
 *  Created on: Jun 27, 2014
 *      Author: Jonas Kunze (kunze.jonas@gmail.com)
 */

#include "HandleFrameTask.h"

#include <glog/logging.h>
#include <l0/MEP.h>
#include <l0/MEPFragment.h>
#include <LKr/LkrFragment.h>
#include <LKr/LKRMEP.h>
#include <net/ethernet.h>
#include <net/if_arp.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <algorithm>
#include <atomic>
#include <cstdbool>
#include <cstdint>
#include <iostream>
#include <vector>

#include <eventBuilding/SourceIDManager.h>
#include <exceptions/UnknownCREAMSourceIDFound.h>
#include <exceptions/UnknownSourceIDFound.h>
#include <utils/DataDumper.h>
#include <options/Options.h>
#include <socket/NetworkHandler.h>
#include <structs/Network.h>

#include "../eventBuilding/L1Builder.h"
#include "../eventBuilding/L2Builder.h"
#include "../options/MyOptions.h"
#include "PacketHandler.h"

namespace na62 {

uint16_t HandleFrameTask::L0_Port;
uint16_t HandleFrameTask::CREAM_Port;
uint16_t HandleFrameTask::EOB_BROADCAST_PORT;
uint32_t HandleFrameTask::MyIP;

uint32_t HandleFrameTask::currentBurstID_;
uint32_t HandleFrameTask::nextBurstID_;

HandleFrameTask::HandleFrameTask(DataContainer&& _container) :
		container(_container) {
}

HandleFrameTask::~HandleFrameTask() {
}

void HandleFrameTask::Initialize() {
	L0_Port = Options::GetInt(OPTION_L0_RECEIVER_PORT);
	CREAM_Port = Options::GetInt(OPTION_CREAM_RECEIVER_PORT);
	EOB_BROADCAST_PORT = Options::GetInt(
	OPTION_EOB_BROADCAST_PORT);
	MyIP = NetworkHandler::GetMyIP();

	currentBurstID_ = Options::GetInt(OPTION_FIRST_BURST_ID);
	nextBurstID_ = currentBurstID_;
}

void HandleFrameTask::processARPRequest(struct ARP_HDR* arp) {
	/*
	 * Look for ARP requests asking for my IP
	 */
	if (arp->targetIPAddr == NetworkHandler::GetMyIP()) { // This is asking for me
		struct DataContainer responseArp = EthernetUtils::GenerateARPv4(
				NetworkHandler::GetMyMac().data(), arp->sourceHardwAddr,
				NetworkHandler::GetMyIP(), arp->sourceIPAddr,
				ARPOP_REPLY);
		NetworkHandler::AsyncSendFrame(std::move(responseArp));
	}
}

tbb::task* HandleFrameTask::execute() {
	try {
		struct UDP_HDR* hdr = (struct UDP_HDR*) container.data;
		uint16_t etherType = ntohs(hdr->eth.ether_type);
		uint8_t ipProto = hdr->ip.protocol;
		uint16_t destPort = ntohs(hdr->udp.dest);
		uint32_t dstIP = hdr->ip.daddr;

		/*
		 * Check if we received an ARP request
		 */
		if (etherType != ETHERTYPE_IP || ipProto != IPPROTO_UDP) {
			if (etherType == ETHERTYPE_ARP) {
				// this will delete the data
				processARPRequest((struct ARP_HDR*) container.data);
				return nullptr;
			} else {
				// Just ignore this frame as it's not IP nor ARP
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

		/*
		 * Check if we are really the destination of the IP datagram
		 */
		if (!MyIP == dstIP) {
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

			/*
			 * If the event has a small number we should check if the burstID is already updated
			 */
			if (nextBurstID_ != currentBurstID_
					&& mep->getFirstEventNum() < 1000) {
				currentBurstID_ = nextBurstID_;
			}

			PacketHandler::MEPsReceivedBySourceID_[mep->getSourceID()]++;
			PacketHandler::EventsReceivedBySourceID_[mep->getSourceID()] +=
					mep->getNumberOfEvents();
			PacketHandler::BytesReceivedBySourceID_[mep->getSourceID()] +=
					container.length;

			for (int i = mep->getNumberOfEvents() - 1; i >= 0; i--) {
				L1Builder::buildEvent(mep->getEvent(i), currentBurstID_);
			}
		} else if (destPort == CREAM_Port) {
			/*
			 * The LKRMEP will not be stored directly. Instead the LkrFragments will store the MEP they
			 * are stored in and also delete it as soon as all Fragments of the MEP are deleted
			 */
			cream::LKRMEP* mep = new cream::LKRMEP(UDPPayload, dataLength,
					container.data);

			PacketHandler::MEPsReceivedBySourceID_[SOURCE_ID_LKr]++;
			PacketHandler::EventsReceivedBySourceID_[SOURCE_ID_LKr] +=
					mep->getNumberOfEvents();
			PacketHandler::BytesReceivedBySourceID_[SOURCE_ID_LKr] +=
					container.length;

			/*
			 * Build events with all MEP fragments
			 * getNumberOfEvents() will change while executing L2Builder::buildEvent. So we have to cache it
			 */
			const uint numberOfStoredEvents = mep->getNumberOfEvents();
			for (uint i = 0; i != numberOfStoredEvents; i++) {
				L2Builder::buildEvent(mep->getEvent(i));
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

			nextBurstID_ = pack->finishedBurstID + 1;
		} else {
			/*
			 * Packet with unknown UDP port received
			 */
			LOG(ERROR) <<"Packet with unknown UDP port received: " << destPort;
			delete[] container.data;
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
