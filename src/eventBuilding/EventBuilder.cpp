/*
 * EventCollector.cpp
 *
 *  Created on: Dec 6, 2011
 *      Author: Jonas Kunze (kunzej@cern.ch)
 */

#include "EventBuilder.h"

#include <arpa/inet.h>
#include <asm-generic/errno-base.h>
#include <boost/thread/detail/thread.hpp>
#include <glog/logging.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <zmq.h>
#include <zmq.hpp>
#include <cstdbool>
#include <iostream>
#include <new>
#include <string>

#include <exceptions/NA62Error.h>
#include <l0/MEPEvent.h>
#include <LKr/L1DistributionHandler.h>
#include <LKr/LKREvent.h>
#include <options/Options.h>
#include <socket/EthernetUtils.h>
#include <socket/PFringHandler.h>
#include <socket/ZMQHandler.h>
#include <structs/Network.h>
#include <structs/Event.h>
#include <eventBuilding/Event.h>
#include <eventBuilding/SourceIDManager.h>

#include <l1/L1TriggerProcessor.h>
#include <l2/L2TriggerProcessor.h>
#include "StorageHandler.h"

namespace na62 {
std::vector<EventBuilder*> EventBuilder::Instances_;

std::atomic<uint64_t>* EventBuilder::L1Triggers_;
std::atomic<uint64_t>* EventBuilder::L2Triggers_;

std::atomic<uint64_t> EventBuilder::BytesSentToStorage_(0);
std::atomic<uint64_t> EventBuilder::EventsSentToStorage_(0);

boost::timer::cpu_timer EventBuilder::EOBReceivedTime_;

uint32_t EventBuilder::currentBurstID_ = 0;

uint EventBuilder::NUMBER_OF_EBS = 0;

EventBuilder::EventBuilder() :
		L0Socket_(ZMQHandler::GenerateSocket(ZMQ_PULL)), LKrSocket_(
				ZMQHandler::GenerateSocket(ZMQ_PULL)), changeBurstID_(
		false), threadCurrentBurstID_(0), L1processor_(new L1TriggerProcessor), L2processor_(
				new L2TriggerProcessor(threadNum_)) {

	Instances_.push_back(this);
}

EventBuilder::~EventBuilder() {
	L0Socket_->close();
	LKrSocket_->close();
	delete L0Socket_;
	delete LKrSocket_;
}

void EventBuilder::Initialize() {
	NUMBER_OF_EBS = Options::GetInt(OPTION_NUMBER_OF_EBS);
	L1Triggers_ = new std::atomic<uint64_t>[0xFF + 1];
	L2Triggers_ = new std::atomic<uint64_t>[0xFF + 1];

	for (int i = 0; i <= 0xFF; i++) {
		L1Triggers_[i] = 0;
		L2Triggers_[i] = 0;
	}
}

void EventBuilder::thread() {
	eventPool_.resize(
			Options::GetInt(OPTION_NUMBER_OF_EVENTS_PER_BURST_EXPECTED));
	for (int i = 0;
			i
					!= Options::GetInt(
							OPTION_NUMBER_OF_EVENTS_PER_BURST_EXPECTED)
							/ NUMBER_OF_EBS; ++i) {
		unusedEvents_.push_back(new Event(0));
	}

	threadCurrentBurstID_ = Options::GetInt(OPTION_FIRST_BURST_ID);

	ZMQHandler::BindInproc(L0Socket_, ZMQHandler::GetEBL0Address(threadNum_));
	ZMQHandler::BindInproc(LKrSocket_, ZMQHandler::GetEBLKrAddress(threadNum_));

	zmq::pollitem_t items[] = { { *L0Socket_, 0, ZMQ_POLLIN, 0 }, { *LKrSocket_,
			0, ZMQ_POLLIN, 0 } };

	while (1) {
		try {
			boost::this_thread::interruption_point();

			zmq::message_t message;
			zmq::poll(&items[0], 2, 1000); // Poll only blocks for 1 s so that we pass interruption_point

			if (items[0].revents & ZMQ_POLLIN) { // L0 data
				L0Socket_->recv(&message);
				handleL0Data((l0::MEPEvent*) message.data());
			} else if (changeBurstID_) {
				if (EOBReceivedTime_.elapsed().wall > 100E6) {
					LOG(INFO)<< "EB "<<threadNum_ <<" changed burst number to " << currentBurstID_ << " (" << (EOBReceivedTime_.elapsed().wall*1E6) << " ms after receiving the EOB)";
					threadCurrentBurstID_ = currentBurstID_;
					changeBurstID_ = false;
				}
			}

			if (items[1].revents & ZMQ_POLLIN) { // LKr data
				LKrSocket_->recv(&message);
				handleLKRData((cream::LKREvent*) message.data());
			}
		} catch (NA62Error &e) {
// Continue... Message will be printed automatically
		} catch (const zmq::error_t& ex) {
			if (ex.num() != EINTR) {
				L0Socket_->close();
				LKrSocket_->close();
				delete L0Socket_;
				delete LKrSocket_;
				return;
			}
		}
	}
}

Event* EventBuilder::getNewEvent(uint32_t eventNumber) {
	if (!unusedEvents_.empty()) {
		Event *event;
		event = unusedEvents_.back();
		unusedEvents_.pop_back();
		event->setEventNumber(eventNumber);
		return event;
	} else {
		return new Event(eventNumber);
	}
}

void EventBuilder::handleL0Data(l0::MEPEvent *mepEvent) {
	/*
	 * Receiver only pushes MEPEVENT::eventNum%EBNum events. To fill all holes in eventPool we need divide by the number of event builder
	 */
	const uint32_t eventPoolIndex = (mepEvent->getEventNumber() / NUMBER_OF_EBS);
	Event *event;

	if (eventPoolIndex >= eventPool_.size()) { // Memory overflow
		eventPool_.resize(eventPoolIndex * 2);
		event = getNewEvent(mepEvent->getEventNumber());
		eventPool_[eventPoolIndex] = event;
	} else {
		event = eventPool_[eventPoolIndex];
		if (event == nullptr) { // An event with a higher eventPoolIndex has been received before this one
			event = getNewEvent(mepEvent->getEventNumber());
			eventPool_[eventPoolIndex] = event;
		}
	}

	/*
	 * Add new packet to Event
	 */
	if (!event->addL0Event(mepEvent, getCurrentBurstID())) {
		return;
	} else {
		/*
		 * This event is complete -> process it
		 */
		processL1(event);
	}
}

void EventBuilder::handleLKRData(cream::LKREvent *lkrEvent) {
	/*
	 * Receiver only pushes MEPEVENT::eventNum%EBNum events. To fill all holes in eventPool we need divide by the number of event builder
	 */
	const uint32_t eventPoolIndex = (lkrEvent->getEventNumber() / NUMBER_OF_EBS);

	if (eventPoolIndex >= eventPool_.size()) {
		throw na62::NA62Error(
				"Received an LKrEvent with ID "
						+ std::to_string(lkrEvent->getEventNumber())
						+ " while there has not been any L0 data received for this event");
	}

	/*
	 * Get the corresponding Event object. It may NOT be NULL (don't delete, call Event::destroy() instead) but may be empty if mepEvent is the first part of this Event.
	 */
	Event *event = eventPool_[eventPoolIndex];

	if (event == nullptr) {
		throw na62::NA62Error(
				"Received an LKrEvent with ID "
						+ std::to_string(lkrEvent->getEventNumber())
						+ " while there has not been any L0 data received for this event");
	}
	/*
	 * Add new packet to EventCollector
	 */
	if (!event->addLKREvent(lkrEvent)) {
// result == false -> subevents are still incomplete
		return;
	} else {
// result == true -> Last missing packet received!
		/*
		 * This event is complete -> process it
		 */
		processL2(event);
	}
}

void EventBuilder::processL1(Event *event) {
	/*
	 * Changing the BurstID will now be done by the dim interface
	 */
	if (event->isLastEventOfBurst()) {
		SendEOBBroadcast(event->getEventNumber(), threadCurrentBurstID_);
	}

	/*
	 * Process Level 1 trigger
	 */
	uint16_t L0L1Trigger = L1processor_->compute(event);
	L1Triggers_[L0L1Trigger >> 8]++; // The second 8 bits are the L1 trigger type word
	event->setL1Processed(L0L1Trigger);

	if (SourceIDManager::NUMBER_OF_EXPECTED_CREAM_PACKETS_PER_EVENT != 0) {
		if (L0L1Trigger != 0) {
			/*
			 * Only request accepted events from LKr
			 */
			sendL1RequestToCREAMS(event);
		}
	} else {
		if (L0L1Trigger != 0) {
			processL2(event);
		}
	}

	/*
	 * If the Event has been rejected by L1 we can destroy it now
	 */
	if (L0L1Trigger == 0) {
		event->destroy();
	}
}

void EventBuilder::processL2(Event * event) {
	if (!event->isWaitingForNonZSuppressedLKrData()) {
		/*
		 * L1 already passed but non zero suppressed LKr data not yet requested -> Process Level 2 trigger
		 */
		uint8_t L2Trigger = L2processor_->compute(event);

		event->setL2Processed(L2Trigger);

		/*
		 * Event has been processed and saved or rejected -> destroy, don't delete so that it can be reused if
		 * during L2 no non zero suppressed LKr data has been requested
		 */
		if (!event->isWaitingForNonZSuppressedLKrData()) {
			if (event->isL2Accepted()) {
				BytesSentToStorage_ += StorageHandler::SendEvent(threadNum_,
						event);
				EventsSentToStorage_++;
			}
			L2Triggers_[L2Trigger]++;
			event->destroy();
		}
	} else {
		uint8_t L2Trigger = L2processor_->onNonZSuppressedLKrDataReceived(
				event);

		event->setL2Processed(L2Trigger);
		if (event->isL2Accepted()) {
			BytesSentToStorage_ += StorageHandler::SendEvent(threadNum_, event);
			EventsSentToStorage_++;
		}
		L2Triggers_[L2Trigger]++;
		event->destroy();
	}
}

void EventBuilder::sendL1RequestToCREAMS(Event* event) {
	cream::L1DistributionHandler::Async_RequestLKRDataMulticast(threadNum_,
			event,
			false);
}

void EventBuilder::SendEOBBroadcast(uint32_t eventNumber,
		uint32_t finishedBurstID) {
	LOG(INFO)<<"Sending EOB broadcast to "
	<< Options::GetString(OPTION_EOB_BROADCAST_IP) << ":"
	<< Options::GetInt(OPTION_EOB_BROADCAST_PORT);
	EOB_FULL_FRAME EOBPacket;

	EOBPacket.finishedBurstID = finishedBurstID;
	EOBPacket.lastEventNum = eventNumber;

	EthernetUtils::GenerateUDP((const char*) &EOBPacket,
			EthernetUtils::StringToMAC("FF:FF:FF:FF:FF:FF"),
			inet_addr(Options::GetString(OPTION_EOB_BROADCAST_IP).data()),
			Options::GetInt(OPTION_EOB_BROADCAST_PORT),
			Options::GetInt(OPTION_EOB_BROADCAST_PORT));

	EOBPacket.udp.setPayloadSize(
			sizeof(struct EOB_FULL_FRAME) - sizeof(struct UDP_HDR));
	EOBPacket.udp.ip.check = 0;
	EOBPacket.udp.ip.check = EthernetUtils::GenerateChecksum(
			(const char*) (&EOBPacket.udp.ip), sizeof(struct iphdr));
	EOBPacket.udp.udp.check = EthernetUtils::GenerateUDPChecksum(&EOBPacket.udp,
			sizeof(struct EOB_FULL_FRAME));

	PFringHandler::SendFrame((char*) &EOBPacket, sizeof(struct EOB_FULL_FRAME),
			true, false);

	EventBuilder::SetNextBurstID(EOBPacket.finishedBurstID + 1);
}

}
/* namespace na62 */
