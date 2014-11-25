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
#include "../straws/StrawReceiver.h"
#include "PacketHandler.h"
#include "FragmentStore.h"

namespace na62 {

uint16_t HandleFrameTask::L0_Port;
uint16_t HandleFrameTask::CREAM_Port;
uint16_t HandleFrameTask::STRAW_PORT;
uint32_t HandleFrameTask::MyIP;

uint32_t HandleFrameTask::currentBurstID_;
uint32_t HandleFrameTask::nextBurstID_;

boost::timer::cpu_timer HandleFrameTask::eobFrameReceivedTime_;
std::atomic<uint> HandleFrameTask::queuedTasksNum_;
uint HandleFrameTask::highestSourceNum_;
std::atomic<uint64_t>* HandleFrameTask::MEPsReceivedBySourceNum_;
std::atomic<uint64_t>* HandleFrameTask::BytesReceivedBySourceNum_;

HandleFrameTask::HandleFrameTask(std::vector<DataContainer>&& _containers) :
		containers(std::move(_containers)) {
	queuedTasksNum_.fetch_add(1, std::memory_order_relaxed);
}

HandleFrameTask::~HandleFrameTask() {
	queuedTasksNum_.fetch_sub(1, std::memory_order_relaxed);
}

void HandleFrameTask::initialize() {
	L0_Port = Options::GetInt(OPTION_L0_RECEIVER_PORT);
	CREAM_Port = Options::GetInt(OPTION_CREAM_RECEIVER_PORT);
	STRAW_PORT = Options::GetInt(OPTION_STRAW_PORT);
	MyIP = NetworkHandler::GetMyIP();

	currentBurstID_ = Options::GetInt(OPTION_FIRST_BURST_ID);
	nextBurstID_ = currentBurstID_;

	/*
	 * All L0 data sources and LKr:
	 */
	highestSourceNum_ = SourceIDManager::NUMBER_OF_L0_DATA_SOURCES;

	MEPsReceivedBySourceNum_ = new std::atomic<uint64_t>[highestSourceNum_ + 1];
	BytesReceivedBySourceNum_ =
			new std::atomic<uint64_t>[highestSourceNum_ + 1];

	for (uint i = 0; i != highestSourceNum_ + 1; i++) {
		MEPsReceivedBySourceNum_[i] = 0;
		BytesReceivedBySourceNum_[i] = 0;
	}
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
	for (DataContainer& container : containers) {
		processFrame(std::move(container));
	}
	return nullptr;
}

void HandleFrameTask::processFrame(DataContainer&& container) {
	try {
		struct UDP_HDR* hdr = (struct UDP_HDR*) container.data;
		const uint16_t etherType = /*ntohs*/(hdr->eth.ether_type);
		const uint8_t ipProto = hdr->ip.protocol;
		uint16_t destPort = ntohs(hdr->udp.dest);
		const uint32_t dstIP = hdr->ip.daddr;

		/*
		 * Check if we received an ARP request
		 */
		if (etherType != 0x0008/*ETHERTYPE_IP*/|| ipProto != IPPROTO_UDP) {
			if (etherType == 0x0608/*ETHERTYPE_ARP*/) {
				// this will delete the data
				processARPRequest((struct ARP_HDR*) container.data);
				return;
			} else {
				// Just ignore this frame as it's neither IP nor ARP
				container.free();
				return;
			}
		}

		/*
		 * Check checksum errors
		 */
		if (!checkFrame(hdr, container.length)) {
			container.free();
			return;
		}

		/*
		 * Check if we are really the destination of the IP datagram
		 */
		if (MyIP != dstIP) {
			container.free();
			return;
		}

		if (hdr->isFragment()) {
			container = FragmentStore::addFragment(std::move(container));
			if (container.data == nullptr) {
				return;
			}
			hdr = (struct UDP_HDR*) container.data;
			destPort = ntohs(hdr->udp.dest);
		}

		const char * UDPPayload = container.data + sizeof(struct UDP_HDR);
		const uint16_t & UdpDataLength = ntohs(hdr->udp.len)
				- sizeof(struct udphdr);

		/*
		 *  Now let's see what's insight the packet
		 */
		if (destPort == L0_Port) { ////////////////////////////////////////////////// L0 Data //////////////////////////////////////////////////
			/*
			 * L0 Data
			 * Length is hdr->ip.tot_len-sizeof(struct udphdr) and not container.length because of ethernet padding bytes!
			 */
			l0::MEP* mep = new l0::MEP(UDPPayload, UdpDataLength,
					container.data);

			/*
			 * If the event has a small number we should check if the burstID is already updated and the update is long enough ago. Otherwise
			 * we would increment the burstID while we are still processing events from the last burst.
			 */
			if (nextBurstID_ != currentBurstID_
			//&& mep->getFirstEventNum() < 1000
					&& eobFrameReceivedTime_.elapsed().wall / 1E6
							> 2000 /*2s*/) {
				currentBurstID_ = nextBurstID_;
			}

			uint sourceNum = SourceIDManager::SourceIDToNum(mep->getSourceID());

			MEPsReceivedBySourceNum_[sourceNum].fetch_add(1,
					std::memory_order_relaxed);
			BytesReceivedBySourceNum_[sourceNum].fetch_add(container.length,
					std::memory_order_relaxed);

			for (int i = mep->getNumberOfEvents() - 1; i >= 0; i--) {
				// Add every fragment
				L1Builder::buildEvent(mep->getFragment(i), currentBurstID_);
			}
		} else if (destPort == CREAM_Port) { ////////////////////////////////////////////////// CREAM Data //////////////////////////////////////////////////
			cream::LkrFragment* fragment = new cream::LkrFragment(UDPPayload,
					UdpDataLength, container.data);

			MEPsReceivedBySourceNum_[highestSourceNum_].fetch_add(1,
					std::memory_order_relaxed);

			BytesReceivedBySourceNum_[highestSourceNum_].fetch_add(
					container.length, std::memory_order_relaxed);

			L2Builder::buildEvent(fragment);
		} else if (destPort == STRAW_PORT) { ////////////////////////////////////////////////// STRAW Data //////////////////////////////////////////////////
			if (nextBurstID_ != currentBurstID_
					&& eobFrameReceivedTime_.elapsed().wall / 1E6
							> 2000 /*2s*/) {
				currentBurstID_ = nextBurstID_;
			}

			StrawReceiver::processFrame(std::move(container), currentBurstID_);
		} else {
			/*
			 * Packet with unknown UDP port received
			 */
			LOG_ERROR<<"Packet with unknown UDP port received: " << destPort << ENDL;
			container.free();
		}
	} catch (UnknownSourceIDFound const& e) {
		container.free();
	} catch (UnknownCREAMSourceIDFound const&e) {
		container.free();
	} catch (NA62Error const& e) {
		container.free();
	}
}

bool HandleFrameTask::checkFrame(struct UDP_HDR* hdr, uint16_t length) {
	/*
	 * Check IP-Header
	 */
	//				if (!EthernetUtils::CheckData((char*) &hdr->ip, sizeof(iphdr))) {
	//					LOG_ERROR << "Packet with broken IP-checksum received");
	//					container.free();
	//					continue;
	//				}
	if (hdr->isFragment()) {
		return true;
	}

	if (ntohs(hdr->ip.tot_len) + sizeof(ether_header) != length) {
		/*
		 * Does not need to be equal because of ethernet padding
		 */
		if (ntohs(hdr->ip.tot_len) + sizeof(ether_header) > length) {
			LOG_ERROR <<
			"Received IP-Packet with less bytes than ip.tot_len field! " <<
			(ntohs(hdr->ip.tot_len) + sizeof(ether_header) ) << ":"<<length << ENDL;
			return false;
		}
	}

	/*
	 * Does not need to be equal because of ethernet padding
	 */
	if (ntohs(hdr->udp.len) + sizeof(ether_header) + sizeof(iphdr) > length) {
		LOG_ERROR<<"Received UDP-Packet with less bytes than udp.len field! "<<(ntohs(hdr->udp.len) + sizeof(ether_header) + sizeof(iphdr)) <<":"<<length;
		return false;
	}

	//				/*
	//				 * Check UDP checksum
	//				 */
	//				if (!EthernetUtils::CheckUDP(hdr, (const char *) (&hdr->udp) + sizeof(struct udphdr), ntohs(hdr->udp.len) - sizeof(struct udphdr))) {
	//					LOG_ERROR << "Packet with broken UDP-checksum received" ) << ENDL;
	//					container.free();
	//					continue;
	//				}
	return true;
}

}
/* namespace na62 */
