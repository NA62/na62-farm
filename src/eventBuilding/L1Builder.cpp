/*
 * HandleFrameTask.cpp
 *
 *  Created on: Jun 27, 2014
 *      Author: Jonas Kunze (kunze.jonas@gmail.com)
 */

#include "L1Builder.h"
#include "L2Builder.h"
#ifdef USE_ERS
#include <exceptions/CommonExceptions.h>
#endif
#include <eventBuilding/Event.h>
#include <eventBuilding/EventPool.h>
#include <eventBuilding/SourceIDManager.h>
#include <l0/MEP.h>
#include <l0/MEPFragment.h>
#include <l0/Subevent.h>
#include <l1/L1DistributionHandler.h>
#include <l1/L1Fragment.h>
#include <l1/L1TriggerProcessor.h>
#include <sys/types.h>
#include <cstdbool>
#include <monitoring/HltStatistics.h>

#ifdef USE_SHAREDMEMORY
#include "SharedMemory/SharedMemoryManager.h"
#endif



namespace na62 {

std::atomic<uint64_t> L1Builder::L1Requests_(0);

std::atomic<uint64_t> L1Builder::L0BuildingTimeCumulative_(0);
std::atomic<uint64_t> L1Builder::L0BuildingTimeMax_(0);
std::atomic<uint64_t> L1Builder::L1ProcessingTimeCumulative_(0);
std::atomic<uint64_t> L1Builder::L1ProcessingTimeMax_(0);
std::atomic<uint64_t>** L1Builder::L0BuildingTimeVsEvtNumber_;
std::atomic<uint64_t>** L1Builder::L1ProcessingTimeVsEvtNumber_;
bool L1Builder::requestZSuppressedLkrData_;

//FIXME: global
bool bypassL1=false;

void L1Builder::buildEvent(l0::MEPFragment* fragment, uint_fast32_t burstID, TaskProcessor* taskProcessor) {
	Event * event = nullptr;

#ifdef USE_ERS
	try {
		event = EventPool::getEvent(fragment->getEventNumber());
	}
	catch (na62::Message &e) {
		ers::error(UnexpectedFragment(ERS_HERE, fragment->getEventNumber(), SourceIDManager::sourceIdToDetectorName( fragment->getSourceID()), fragment->getSourceSubID(), e));
		delete fragment;
		return;
	}
#else
	/*
	 * If the event number is too large event is null and we have to drop the data
	 */
	event = EventPool::getEvent(fragment->getEventNumber());

	if (event == nullptr) {
		LOG_ERROR(
				"type = BadEv : Eliminating " << (int)(fragment->getEventNumber()) << " from source " << std::hex << (int)(fragment->getSourceID()) << ":" << (int)(fragment->getSourceSubID()) << std::dec);

		delete fragment;
		return;
	}
#endif

	/*
	 * Add new packet to Event
	 */

	if (event->addL0Fragment(fragment, burstID)) {

		/*
		 * Store the global event timestamp taken from the reverence detector
		 */
		l0::MEPFragment* tsFragment = event->getL0SubeventBySourceIDNum(SourceIDManager::TS_SOURCEID_NUM)->getFragment(0);
		event->setTimestamp(tsFragment->getTimestamp());

#ifdef MEASURE_TIME
		uint L0BuildingTimeIndex = (uint) event->getL0BuildingTime() / 5000.;
		//uint L0BuildingTimeIndex = (uint) (event->getL0BuildingTime() / 1000. + 0.5);
		if (L0BuildingTimeIndex >= 0x64) {
			L0BuildingTimeIndex = 0x64;
		}
		uint EventTimestampIndex = (uint) ((event->getTimestamp() * 25e-8));
		if (EventTimestampIndex >= 0x64) {
			EventTimestampIndex = 0x64;
		}
		L0BuildingTimeVsEvtNumber_[L0BuildingTimeIndex][EventTimestampIndex].fetch_add(1, std::memory_order_relaxed);

		L0BuildingTimeCumulative_.fetch_add(event->getL0BuildingTime(), std::memory_order_relaxed);
		if (event->getL0BuildingTime() >= L0BuildingTimeMax_) {
			L0BuildingTimeMax_ = event->getL0BuildingTime();
		}
#endif
		event->readTriggerTypeWordAndFineTime();
		/*
		 * This event is complete -> process it
		 */
		processL1(event, taskProcessor);

	}
	return;
}

void L1Builder::processL1(Event *event, TaskProcessor* taskProcessor) {

#ifdef USE_SHAREDMEMORY
	/*
	 * Send L1 to trigger processor
	 */
	bypassL1 = false;
	if (SharedMemoryManager::storeL1Event(event)) {
		//Counting just event successfully sent in the shared memory
		//LOG_ERROR("Serialized on the shared memory");
		uint amount = 1;
		SharedMemoryManager::setEventOut(event->getBurstID(), amount);

	} else {
		//TODO unable to store on the shared memory can process it locally?
		//LOG_ERROR("Unable to serialize event " << (int) event->getEventNumber()  << " on the shared memory. -> send L1 request");
		// send L1 request!
		uint_fast8_t l1TriggerTypeWord = 0x20;
		uint_fast8_t l0TriggerTypeWord = event->getL0TriggerTypeWord(); //special trigger word to mark these events
		HltStatistics::updateL1Statistics(event, l1TriggerTypeWord);

		uint_fast16_t L0L1Trigger(l0TriggerTypeWord | l1TriggerTypeWord << 8);
		event->setL1Processed(L0L1Trigger);
		if (SourceIDManager::NUMBER_OF_EXPECTED_L1_PACKETS_PER_EVENT != 0) {
				sendL1Request(event);
				event->setL1Requested();
				SharedMemoryManager::setEventL1Requested(event->getBurstID(), 1);
		}
		else {
				L2Builder::processL2(event);
		}
	}

#else

	/*
	 * Process Level 1 trigger
	 */

	uint_fast8_t l0TriggerTypeWord = event->getL0TriggerTypeWord();
	uint_fast8_t l1TriggerTypeWord = L1TriggerProcessor::compute(event, taskProcessor->getStrawAlgo());
	/*STATISTICS*/
	HltStatistics::updateL1Statistics(event, l1TriggerTypeWord);

	uint_fast16_t L0L1Trigger(l0TriggerTypeWord | l1TriggerTypeWord << 8);
	event->setL1Processed(L0L1Trigger);

#ifdef MEASURE_TIME
	uint L1ProcessingTimeIndex = (uint) event->getL1ProcessingTime() / 10;
	//uint L1ProcessingTimeIndex = (uint) event->getL1ProcessingTime() + 0.5;
	if (L1ProcessingTimeIndex >= 0x64) {
		L1ProcessingTimeIndex = 0x64;
	}
	uint EventTimestampIndex = (uint) ((event->getTimestamp() * 25e-08));
	//uint EventTimestampIndex = (uint) (event->getTimestamp() * 25e-8);
	if (EventTimestampIndex >= 0x64) {
		EventTimestampIndex = 0x64;
	}
	L1ProcessingTimeVsEvtNumber_[L1ProcessingTimeIndex][EventTimestampIndex].fetch_add(1, std::memory_order_relaxed);
	L1ProcessingTimeCumulative_.fetch_add(event->getL1ProcessingTime(), std::memory_order_relaxed);
	if (event->getL1ProcessingTime() >= L1ProcessingTimeMax_) {
		L1ProcessingTimeMax_ = event->getL1ProcessingTime();
	}
#endif
	if (l1TriggerTypeWord != 0) {

		if (SourceIDManager::NUMBER_OF_EXPECTED_L1_PACKETS_PER_EVENT != 0) {
			sendL1Request(event);
		} else {
			L2Builder::processL2(event);
		}
	} else { // Event not accepted
		/*
		 * If the Event has been rejected by L1 we can destroy it now
		 */
		EventPool::freeEvent(event);
	}
#endif
}

void L1Builder::sendL1Request(Event* event) {

	// See https://github.com/NA62/na62-trigger-algorithms/wiki/CREAM-data
	l1::L1DistributionHandler::Async_RequestL1DataMulticast(event,
			event->isRrequestZeroSuppressedCreamData() && requestZSuppressedLkrData_);
	L1Requests_.fetch_add(1, std::memory_order_relaxed);

	HltStatistics::sumCounter("L1RequestToCreams", 1);
}
}
/* namespace na62 */
