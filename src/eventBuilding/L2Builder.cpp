/*
 * HandleFrameTask.cpp
 *
 *  Created on: Jun 27, 2014
 *      Author: Jonas Kunze (kunze.jonas@gmail.com)
 */

#include "L2Builder.h"

#include <eventBuilding/Event.h>
#include <eventBuilding/EventPool.h>
#include <l0/MEPFragment.h>
#include <l0/MEP.h>
#include <l0/Subevent.h>
#include <l2/L2Fragment.h>
#include <l2/L2TriggerProcessor.h>
#include <l1/MEP.h>
#include <l1/MEPFragment.h>
#include <netinet/ip.h>
#include <structs/Network.h>
#include <sys/types.h>
#include <cstdbool>
#ifdef USE_ERS
#include <exceptions/CommonExceptions.h>
#endif
#include "StorageHandler.h"
#include "SharedMemory/SharedMemoryManager.h"
#include <monitoring/HltStatistics.h>
#include <structs/LkrCrateSlotDecoder.h>

namespace na62 {

std::atomic<uint64_t> L2Builder::L1BuildingTimeCumulative_(0);
std::atomic<uint64_t> L2Builder::L1BuildingTimeMax_(0);
std::atomic<uint64_t> L2Builder::L2ProcessingTimeCumulative_(0);
std::atomic<uint64_t> L2Builder::L2ProcessingTimeMax_(0);
//std::atomic<uint64_t> L2Builder::BytesSentToStorage_(0);

//std::atomic<uint64_t> L2Builder::EventsSentToStorage_(0);

std::atomic<uint64_t>** L2Builder::L1BuildingTimeVsEvtNumber_;
std::atomic<uint64_t>** L2Builder::L2ProcessingTimeVsEvtNumber_;

void L2Builder::buildEvent(l1::MEPFragment* fragment) {
	Event * event = nullptr;

#ifdef USE_ERS
	try {
		event = EventPool::getEvent(fragment->getEventNumber());
	} catch (na62::Message &e) {
		uint16_t source_sub_id = fragment->getSourceSubID();
		if (SourceIDManager::sourceIdToDetectorName(fragment->getSourceID()) == "LKR") {
			lkr_crate_slot_decoder crateslot(source_sub_id);
			ers::error(UnexpectedFragmentLKR(ERS_HERE, fragment->getEventNumber(), SourceIDManager::sourceIdToDetectorName(fragment->getSourceID()), crateslot.getCrate(), crateslot.getSlot(), e));
		} else {
			ers::error(UnexpectedFragment(ERS_HERE, fragment->getEventNumber(), SourceIDManager::sourceIdToDetectorName(fragment->getSourceID()), source_sub_id, e));
		}

		delete fragment;
		return;
	}
#else
	event = EventPool::getEvent(fragment->getEventNumber());

	/*
	 * If the event number is too large event is null and we have to drop the data
	 */
	if (event == nullptr) {

		uint crateID = (fragment->getSourceSubID() >> 5) & 0x3f;
		uint creamID = fragment->getSourceSubID() & 0x1f;

		LOG_ERROR(
				"type = BadEv : Eliminating " << std::hex << (int)(fragment->getEventNumber()) << " from source = 0x" << std::hex << (int)(fragment->getSourceID()) << ":0x" << (int)(fragment->getSourceSubID()) << std::dec << " -- "<< crateID << "--" << creamID);
		delete fragment;
		return;
	}
#endif

	/*
	 * Add new packet to EventCollector
	 */

	if (event->addL1Fragment(fragment)) {
#ifdef MEASURE_TIME
		uint L1BuildingTimeIndex = (uint) event->getL1BuildingTime() / 10000.;
		if (L1BuildingTimeIndex >= 0x64)
			L1BuildingTimeIndex = 0x64;
		uint EventTimestampIndex = (uint) ((event->getTimestamp() * 25e-08) / 2);
		if (EventTimestampIndex >= 0x64)
			EventTimestampIndex = 0x64;
		L1BuildingTimeVsEvtNumber_[L1BuildingTimeIndex][EventTimestampIndex].fetch_add(
				1, std::memory_order_relaxed);
		L1BuildingTimeCumulative_.fetch_add(event->getL1BuildingTime(),
				std::memory_order_relaxed);
		if (event->getL0BuildingTime() >= L1BuildingTimeMax_)
			L1BuildingTimeMax_ = event->getL1BuildingTime();
#endif

		/*
		 * This event is complete -> process it
		 */

		processL2(event);
	}
	return;
}

void L2Builder::processL2(Event *event) {

	// If L2 is disabled just write out the event
	if (!SourceIDManager::isL2Active()) {
		event->setL2Processed(0);

//		BytesSentToStorage_.fetch_add(StorageHandler::SendEvent(event),
//				std::memory_order_relaxed);
//		EventsSentToStorage_.fetch_add(1, std::memory_order_relaxed);
		uint64_t BytesSentToStorage = StorageHandler::SendEvent(event);
		HltStatistics::updateStorageStatistics(BytesSentToStorage);
	} else {
		if (!event->isWaitingForNonZSuppressedLKrData()) {
			/*
			 * L1 already passed but non zero suppressed LKr data not yet requested -> Process Level 2 trigger
			 */
			uint_fast8_t L2Trigger = L2TriggerProcessor::compute(event);

			/*STATISTICS*/
			HltStatistics::updateL2Statistics(event, L2Trigger);

#ifdef MEASURE_TIME
			uint L2ProcessingTimeIndex = (uint) event->getL2ProcessingTime() / 1.;
			if (L2ProcessingTimeIndex >= 0x64)
				L2ProcessingTimeIndex = 0x64;
			uint EventTimestampIndex = (uint) ((event->getTimestamp() * 25e-08) / 2);
			if (EventTimestampIndex >= 0x64)
				EventTimestampIndex = 0x64;
			L2ProcessingTimeVsEvtNumber_[L2ProcessingTimeIndex][EventTimestampIndex].fetch_add(
					1, std::memory_order_relaxed);
#endif
			event->setL2Processed(L2Trigger);
#ifdef MEASURE_TIME
			L2ProcessingTimeCumulative_.fetch_add(event->getL2ProcessingTime(),
					std::memory_order_relaxed);
			if (event->getL2ProcessingTime() >= L2ProcessingTimeMax_)
				L2ProcessingTimeMax_ = event->getL2ProcessingTime();
#endif
			/*
			 * Event has been processed and saved or rejected -> destroy, don't delete so that it can be reused if
			 * during L2 no non zero suppressed LKr data has been requested
			 */
			if (!event->isWaitingForNonZSuppressedLKrData()) {
				if (event->isL2Accepted()) {
					/*
					 * Send Event to merger
					 */
					uint64_t BytesSentToStorage = StorageHandler::SendEvent(event);
					//EventsSentToStorage_.fetch_add(1, std::memory_order_relaxed);
					/*STATISTICS*/
					HltStatistics::updateStorageStatistics(BytesSentToStorage);

#ifdef USE_SHAREDMEMORY
					SharedMemoryManager::setEventL1Stored(event->getBurstID(), 1);
#endif
				}
			}
		} else { // Process non zero-suppressed data (not used at the moment!
			// When the implementation will be completed, we need to propagate the L2 downscaling
			uint_fast8_t L2Trigger =
					L2TriggerProcessor::onNonZSuppressedLKrDataReceived(event);

			event->setL2Processed(L2Trigger);
#ifdef MEASURE_TIME
			L2ProcessingTimeCumulative_.fetch_add(event->getL2ProcessingTime(),
					std::memory_order_relaxed);
			if (event->getL2ProcessingTime() >= L2ProcessingTimeMax_)
				L2ProcessingTimeMax_ = event->getL2ProcessingTime();
#endif
			if (event->isL2Accepted()) {

				//BytesSentToStorage_.fetch_add(StorageHandler::SendEvent(event),
				//		std::memory_order_relaxed);
				//EventsSentToStorage_.fetch_add(1, std::memory_order_relaxed);
			}
		}
	}

	// Whater this event was... it's time to eliminate it
	EventPool::freeEvent(event);
}

}
/* namespace na62 */
