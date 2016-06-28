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
#include <l1/MEP.h>
#include <l1/MEPFragment.h>
#include <l1/L1Fragment.h>
#include <l2/L2Fragment.h>
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
#include "PacketHandler.h"
#include "FragmentStore.h"

namespace na62 {

uint_fast16_t HandleFrameTask::L0_Port;
uint_fast16_t HandleFrameTask::CREAM_Port;
//uint_fast16_t HandleFrameTask::STRAW_PORT;
uint_fast32_t HandleFrameTask::MyIP;

std::atomic<uint> HandleFrameTask::queuedTasksNum_;
uint HandleFrameTask::highestSourceNum_;
uint HandleFrameTask::highestL1SourceNum_;
std::atomic<uint64_t>* HandleFrameTask::MEPsReceivedBySourceNum_;
std::atomic<uint64_t>* HandleFrameTask::BytesReceivedBySourceNum_;
std::atomic<uint64_t>* HandleFrameTask::L1MEPsReceivedBySourceNum_;
std::atomic<uint64_t>* HandleFrameTask::L1BytesReceivedBySourceNum_;

HandleFrameTask::HandleFrameTask(std::vector<DataContainer>&& _containers,
		uint burstID) :
		containers_(std::move(_containers)), burstID_(burstID) {
	queuedTasksNum_.fetch_add(1, std::memory_order_relaxed);
}

HandleFrameTask::~HandleFrameTask() {
	queuedTasksNum_.fetch_sub(1, std::memory_order_relaxed);
}

void HandleFrameTask::initialize() {
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



void HandleFrameTask::execute() {


//	while (BurstIdHandler::isEobProcessingRunning()) {
//		usleep(1);
//	}

	for (DataContainer& container : containers_) {
		//If we must clean up the burst we just drop data
		if(BurstIdHandler::flushBurst()) {
			LOG_WARNING("Dropping data because we are at EoB");
			container.free();
		}
		else {
			processFrame(std::move(container));

		}
	}

//	if (queuedTasksNum_ == 1) {
//		BurstIdHandler::checkBurstFinished();
//	}

	//return nullptr;
}

void HandleFrameTask::processFrame(DataContainer&& container) {

	in_port_t destPort = container.UDPPort;
	in_addr_t srcAddr = container.UDPAddr;//hdr->ip.daddr;
	const char * UDPPayload = container.data;// + sizeof(UDP_HDR);
	const uint_fast16_t & UdpDataLength = container.length;//sizeof(container.data);//ntohs(hdr->udp.len)- sizeof(udphdr);

	try {

		/*
		 *  Now let's see what's insight the packet
		 */
		//if (destPort == L0_Port) { ////////////////////////////////////////////////// L0 Data //////////////////////////////////////////////////
			/*
			 * L0 Data
			 * Length is hdr->ip.tot_len-sizeof(udphdr) and not container.length because of ethernet padding bytes!
			 */
			l0::MEP* mep = new l0::MEP(UDPPayload, UdpDataLength, container);

			uint sourceNum = SourceIDManager::sourceIDToNum(mep->getSourceID());

			MEPsReceivedBySourceNum_[sourceNum].fetch_add(1,
					std::memory_order_relaxed);

			BytesReceivedBySourceNum_[sourceNum].fetch_add(container.length,
					std::memory_order_relaxed);


			/*
			 * Setup L1 block if L1 is active copying informations from L0TP MEps
			 */
			if (mep->getSourceID() == SOURCE_ID_L0TP) {
				if (SourceIDManager::isL1Active()) {

					//LOG_INFO("Invent L1 MEP for event " << mep->getFirstEventNum());
					uint16_t mep_factor = mep->getNumberOfFragments();
					uint16_t fragmentLength = sizeof(L1_BLOCK) + 8; //event length in bytes

					const uint32_t L1BlockLength = mep_factor * fragmentLength + 8; //L1 block length in bytes
					char * L1Data = new char[L1BlockLength]; //include UDP header
					l0::MEP_HDR * L1Hdr = (l0::MEP_HDR *) (L1Data + sizeof(UDP_HDR));

					// set MEP header
					L1Hdr->firstEventNum = mep->getFirstEventNum();
					L1Hdr->sourceID = SOURCE_ID_L1;
					L1Hdr->mepLength = L1BlockLength;
					L1Hdr->eventCount = mep_factor;
					L1Hdr->sourceSubID = 0;

					char * virtualFragment = L1Data + 8 /* mep header */;
					l0::MEPFragment * L1Fragment;
					for (uint i = 0; i != mep_factor; i++) {
						L1Fragment = mep->getFragment(i);
						// copy the fragment header
						memcpy(virtualFragment,
								L1Fragment->getDataWithMepHeader(), 8);
						uint16_t temp;
						temp = *(uint16_t *) (virtualFragment) & 0xffff0000;
						temp |= fragmentLength;
						*(uint16_t *) (virtualFragment) = temp;
						virtualFragment += fragmentLength;
					}

					l0::MEP* mep_L1 = new l0::MEP(L1Data,
								    L1BlockLength, { L1Data, L1BlockLength, true, 0 , 0 });

					uint sourceNum = SourceIDManager::sourceIDToNum(
							mep_L1->getSourceID());

					MEPsReceivedBySourceNum_[sourceNum].fetch_add(1,
							std::memory_order_relaxed);
					BytesReceivedBySourceNum_[sourceNum].fetch_add(
							L1BlockLength,
							std::memory_order_relaxed);
#ifndef SIMU
					for (uint i = 0; i != mep_factor; i++) {
						 //Add every fragment
						L1Builder::buildEvent(mep_L1->getFragment(i), burstID_);
					}
#endif
				}
				//Is this part used somewhere?
				if (SourceIDManager::isL2Active()) {
					//LOG_INFO("Invent L2 MEP for event " << mep->getFirstEventNum());
					uint16_t mep_factor = mep->getNumberOfFragments();
					uint32_t L2EventLength = sizeof(L2_BLOCK) + 8; //event length in bytes
					uint32_t L2BlockLength = mep_factor * L2EventLength + 8; //L2 block length in bytes

					char * L2Data = new char[L2BlockLength];
					l0::MEP_HDR * L2Hdr = (l0::MEP_HDR *) (L2Data);

					L2Hdr->firstEventNum = mep->getFirstEventNum();
					L2Hdr->sourceID = SOURCE_ID_L2;
					L2Hdr->mepLength = L2BlockLength;
					L2Hdr->eventCount = mep_factor;
					L2Hdr->sourceSubID = 0;

					char * L2Event = L2Data + 8;
					l0::MEPFragment * L2Fragment;
					for (uint i = 0; i != mep_factor; i++) {
						L2Fragment = mep->getFragment(i);
						memcpy(L2Event, L2Fragment->getDataWithMepHeader(), 8);
						uint temp;
						temp = *(uint *) (L2Event) & 0xffff0000;
						temp |= L2EventLength;
						*(uint *) (L2Event) = temp;
						L2Event += L2EventLength;
					}

					const uint_fast16_t & L2DataLength = L2BlockLength;

					l0::MEP* mep_L2 = new l0::MEP(L2Data,
							        L2DataLength, { L2Data, L2DataLength, true, 0, 0 });
					uint sourceNum = SourceIDManager::sourceIDToNum(
							mep_L2->getSourceID());

					MEPsReceivedBySourceNum_[sourceNum].fetch_add(1,
							std::memory_order_relaxed);
					BytesReceivedBySourceNum_[sourceNum].fetch_add(
							L2BlockLength, std::memory_order_relaxed);
#ifndef SIMU
					for (uint i = 0; i != mep_factor; i++) {
						// Add every fragment
						L1Builder::buildEvent(mep_L2->getFragment(i), burstID_);
					}
#endif
				}
				if (SourceIDManager::isNSTDActive()) {
					//Is this part used somewhere?
					//LOG_INFO("Invent NSTD MEP for event " << mep->getFirstEventNum());
					uint16_t mep_factor = mep->getNumberOfFragments();
					uint32_t NSTDEventLength = sizeof(L2_BLOCK) + 8; //event length in bytes
					uint32_t NSTDBlockLength = mep_factor * NSTDEventLength + 8; //L2 block length in bytes
					char * NSTDData =
							new char[NSTDBlockLength];
					l0::MEP_HDR * NSTDHdr = (l0::MEP_HDR *) (NSTDData);

					NSTDHdr->firstEventNum = mep->getFirstEventNum();
					NSTDHdr->sourceID = SOURCE_ID_NSTD;
					NSTDHdr->mepLength = NSTDBlockLength;
					NSTDHdr->eventCount = mep_factor;
					NSTDHdr->sourceSubID = 0;

					char * NSTDEvent = NSTDData + 8;
					l0::MEPFragment * NSTDFragment;
					for (uint i = 0; i != mep_factor; i++) {
						NSTDFragment = mep->getFragment(i);
						memcpy(NSTDEvent, NSTDFragment->getDataWithMepHeader(),
								8);
						uint temp;
						temp = *(uint *) (NSTDEvent) & 0xffff0000;
						temp |= NSTDEventLength;
						*(uint *) (NSTDEvent) = temp;
						NSTDEvent += NSTDEventLength;
					}

					const uint_fast16_t & NSTDDataLength = NSTDBlockLength;

					l0::MEP* mep_NSTD = new l0::MEP(NSTDData,
							NSTDDataLength, { NSTDData, NSTDDataLength, true, 0, 0 });
					uint sourceNum = SourceIDManager::sourceIDToNum(
							mep_NSTD->getSourceID());

					MEPsReceivedBySourceNum_[sourceNum].fetch_add(1,
							std::memory_order_relaxed);
					BytesReceivedBySourceNum_[sourceNum].fetch_add(
							NSTDBlockLength,
							std::memory_order_relaxed);
#ifndef SIMU
					for (uint i = 0; i != mep_factor; i++) {
						// Add every fragment
						L1Builder::buildEvent(mep_NSTD->getFragment(i), burstID_);
					}
#endif
				}
			}


			uint maxFrags =  mep->getNumberOfFragments();

			for (uint i = 0; i != maxFrags; i++) {
				// Add every fragment
				L1Builder::buildEvent(mep->getFragment(i), burstID_);
			}

	//	} else {
			/*
			 * Packet with unknown UDP port received
			 */
		//	LOG_WARNING("type = BadPack : Packet with unknown UDP port received: " << destPort);
		//	container.free();
		//}
#ifdef USE_ERS
	} catch (UnknownSourceID const& e) {
		//ers::warning(e);
		LOG_ERROR("Unknown source ID received from " + EthernetUtils::ipToString(srcAddr) + ": " + e.message() );
		container.free();
	} catch (CorruptedMEP const&e) {
		//ers::warning(CorruptedMEP(ERS_HERE, "DataSender=" + EthernetUtils::ipToString(srcAddr), e));
		LOG_ERROR("Corruptep received from " + EthernetUtils::ipToString(srcAddr) + ": " + e.message() );
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
