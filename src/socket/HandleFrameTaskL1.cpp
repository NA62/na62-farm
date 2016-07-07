/*
 * HandleFrameTaskL1.cpp
 *
 *  Created on: Jun 23, 2016
 *      Author: Julio Calvo
 */


#include "HandleFrameTaskL1.h"

#include <glog/logging.h>
#include <l0/MEP.h>
#include <l0/MEPFragment.h>
#include <l1/MEP.h>
#include <l1/MEPFragment.h>
#include <l1/L1Fragment.h>
#include <l2/L2Fragment.h>
#include <l1/L1TriggerProcessor.h>
#include <l2/L2TriggerProcessor.h>
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
#ifdef USE_ERS
#include <exceptions/CommonExceptions.h>
#endif
#include <eventBuilding/SourceIDManager.h>
#include <exceptions/BrokenPacketReceivedError.h>
#include <exceptions/UnknownSourceIDFound.h>
#include <utils/DataDumper.h>
#include <options/Options.h>
#include <monitoring/BurstIdHandler.h>
#include <socket/NetworkHandler.h>
#include <structs/Network.h>
#include <socket/EthernetUtils.h>

#include "../eventBuilding/L1Builder.h"
#include "../eventBuilding/L2Builder.h"
#include "../options/MyOptions.h"
//#include "../straws/StrawReceiver.h"
#include "PacketHandlerL1.h"
#include "FragmentStore.h"

namespace na62 {

uint_fast16_t HandleFrameTaskL1::L0_Port;
uint_fast16_t HandleFrameTaskL1::CREAM_Port;
//uint_fast16_t HandleFrameTask::STRAW_PORT;
uint_fast32_t HandleFrameTaskL1::MyIP;

std::atomic<uint> HandleFrameTaskL1::queuedTasksNum_;
uint HandleFrameTaskL1::highestSourceNum_;
uint HandleFrameTaskL1::highestL1SourceNum_;
std::atomic<uint64_t>* HandleFrameTaskL1::MEPsReceivedBySourceNum_;
std::atomic<uint64_t>* HandleFrameTaskL1::BytesReceivedBySourceNum_;
std::atomic<uint64_t>* HandleFrameTaskL1::L1MEPsReceivedBySourceNum_;
std::atomic<uint64_t>* HandleFrameTaskL1::L1BytesReceivedBySourceNum_;

HandleFrameTaskL1::HandleFrameTaskL1(std::vector<DataContainer>&& _containers,
		uint burstID) :
		containers_(std::move(_containers)), burstID_(burstID) {
	queuedTasksNum_.fetch_add(1, std::memory_order_relaxed);
}

HandleFrameTaskL1::~HandleFrameTaskL1() {
	queuedTasksNum_.fetch_sub(1, std::memory_order_relaxed);
}

void HandleFrameTaskL1::initialize() {
	L0_Port = Options::GetInt(OPTION_L0_RECEIVER_PORT);
	CREAM_Port = Options::GetInt(OPTION_CREAM_RECEIVER_PORT);
//	STRAW_PORT = Options::GetInt(OPTION_STRAW_PORT);
	MyIP = NetworkHandler::GetMyIP();

	/*
	 * All L0 data sources
	 */
	highestSourceNum_ = SourceIDManager::NUMBER_OF_L0_DATA_SOURCES;

	MEPsReceivedBySourceNum_ = new std::atomic<uint64_t>[highestSourceNum_];
	BytesReceivedBySourceNum_ =
			new std::atomic<uint64_t>[highestSourceNum_];

	for (uint i = 0; i != highestSourceNum_; i++) {
		MEPsReceivedBySourceNum_[i] = 0;
		BytesReceivedBySourceNum_[i] = 0;
	}

	/*
	 * All L1 data sources
	 */
	highestL1SourceNum_ = SourceIDManager::NUMBER_OF_L1_DATA_SOURCES;

	L1MEPsReceivedBySourceNum_ = new std::atomic<uint64_t>[highestL1SourceNum_];
	L1BytesReceivedBySourceNum_ =
			new std::atomic<uint64_t>[highestL1SourceNum_];

	for (uint i = 0; i != highestL1SourceNum_; i++) {
		L1MEPsReceivedBySourceNum_[i] = 0;
		L1BytesReceivedBySourceNum_[i] = 0;
	}

}


void HandleFrameTaskL1::execute() {


	for (DataContainer& container : containers_) {
		//If we must clean up the burst we just drop data
		if(BurstIdHandler::flushBurst()) {
			LOG_WARNING("Dropping data because we are at EoB");
			container.free();


		}else {
			processFrame(std::move(container));
		}

	}
}



void HandleFrameTaskL1::processFrame(DataContainer&& container) {



	in_port_t destPort = container.UDPPort;
	in_addr_t srcAddr = container.UDPAddr;//hdr->ip.daddr;
	const char * UDPPayload = container.data;// + sizeof(UDP_HDR);
	const uint_fast16_t & UdpDataLength = container.length;//sizeof(container.data);//ntohs(hdr->udp.len)- sizeof(udphdr);

	try {

			l1::MEP* l1mep = new l1::MEP(UDPPayload, UdpDataLength, container);
			//fragment
			uint sourceNum = SourceIDManager::l1SourceIDToNum(l1mep->getSourceID());

			L1MEPsReceivedBySourceNum_[sourceNum].fetch_add(1, std::memory_order_relaxed);

			L1BytesReceivedBySourceNum_[sourceNum].fetch_add(container.length, std::memory_order_relaxed);

//			if (EventPool::getPoolSize() > fragment->getEventNumber()) {
//				EventPool::getCREAMPacketCounter()[fragment->getEventNumber()].fetch_add(
//						1, std::memory_order_relaxed);
//			}
			uint nfrags = l1mep->getNumberOfEvents();
			for (uint i=0; i!= nfrags ; ++i) {
				L2Builder::buildEvent(l1mep->getEvent(i));
			}
		//} else if (destPort == STRAW_PORT) { ////////////////////////////////////////////////// STRAW Data //////////////////////////////////////////////////
		//	StrawReceiver::processFrame(std::move(container), burstID_);
		//} else {
			/*
			 * Packet with unknown UDP port received
			 */
		//	LOG_WARNING("type = BadPack : Packet with unknown UDP port received: " << destPort);
		//	container.free();
	//	}
#ifdef USE_ERS
	} catch (UnknownSourceID const& e) {
		//ers::warning(e);
		LOG_ERROR("Unknown source ID received from " + EthernetUtils::ipToString(srcAddr) + ": " + e.message() );
		container.free();
	} catch (CorruptedMEP const&e) {
		//ers::warning(CorruptedMEP(ERS_HERE, "DataSender=" + EthernetUtils::ipToString(srcAddr), e));
		LOG_ERROR("Corrupted received from " + EthernetUtils::ipToString(srcAddr) + ": " + e.message() );
		container.free();
	} catch (Message const& e) {
		//ers::warning(e);
		LOG_ERROR("Bad data received from " + EthernetUtils::ipToString(srcAddr) + ": " + e.message() );
		container.free();
	}
#else
} catch (UnknownSourceIDFound const& e) {
		container.free();
} catch (BrokenPacketReceivedError const&e) {
		container.free();
} catch (NA62Error const& e) {
		container.free();
}
#endif
}
}


/* namespace na62 */



