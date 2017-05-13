/*
 * QueueReceiver.cpp
 *
 *  Created on: May 16, 2016
 *      Author: marco
 */
#include "SharedMemory/SharedMemoryManager.h"
#include "QueueReceiver.h"
#include "structs/TriggerMessager.h"
#include "structs/SerialEvent.h"

#include "../eventBuilding/L2Builder.h"
#include "../eventBuilding/L1Builder.h"

#include <eventBuilding/EventPool.h>
#include <eventBuilding/Event.h>
#include <monitoring/BurstIdHandler.h>
#include <monitoring/HltStatistics.h>

#include <l1/L1TriggerProcessor.h>

namespace na62 {

uint QueueReceiver::highest_burst_id_received_;

QueueReceiver::QueueReceiver() {
	running_ = true;
	highest_burst_id_received_ = 0;
}

QueueReceiver::~QueueReceiver() {
}

void QueueReceiver::thread() {
	uint event_received_per_burst = 0;
	while (running_) {
		TriggerMessager trigger_message;
		uint priority = 0;

		//Receiving Response
		if (SharedMemoryManager::popTriggerResponseQueue(trigger_message, priority)) {
			//LOG_INFO("Queue Receiver Received trigger response of event: "<<trigger_message.event_id);

			if (trigger_message.burst_id != BurstIdHandler::getCurrentBurstId()) {
				LOG_ERROR("Receiving data belonging to burst id: " << trigger_message.burst_id<<" Skipping...");
				continue;
			}

			//Handling counters for L1 event processed
			if (highest_burst_id_received_ < trigger_message.burst_id) {
				LOG_INFO("########################Received from burst "<< highest_burst_id_received_ << " : " << event_received_per_burst);
				SharedMemoryManager::showLastBurst(20);

				highest_burst_id_received_ = trigger_message.burst_id;
				event_received_per_burst = 0;
			}
			event_received_per_burst++;

			//Counting event arrived in time
			uint amount = 1;
			SharedMemoryManager::setEventIn(trigger_message.burst_id, amount);

			if (trigger_message.level == 1) {

				//printf("l0 trigger flags %d \n", trigger_message.l1_trigger_type_word);

				//Fetching the l0 word
				Event* event = EventPool::getEvent(trigger_message.event_id);
				uint_fast8_t l0TriggerTypeWord = event->getL0TriggerTypeWord();

				uint_fast16_t L0L1Trigger(l0TriggerTypeWord | trigger_message.l1_trigger_type_word << 8);

				event->setRrequestZeroSuppressedCreamData(trigger_message.isRequestZeroSuppressed);
				event->setL1TriggerWords(trigger_message.l1TriggerWords);
				//Writing L0 info
				L1TriggerProcessor::writeL1Data(event, &trigger_message.l1Info, trigger_message.isL1WhileTimeout);
				event->setL1Processed(L0L1Trigger);

				/*STATISTICS*/
				HltStatistics::updateL1Statistics(event, trigger_message.l1_trigger_type_word);

				if (trigger_message.l1_trigger_type_word != 0) {
					if (SourceIDManager::NUMBER_OF_EXPECTED_L1_PACKETS_PER_EVENT != 0) {
						//LOG_ERROR("Sending L1 Request for: " << event->getEventNumber() <<" !");
						L1Builder::sendL1Request(event);
						event->setL1Requested();
						SharedMemoryManager::setEventL1Requested(event->getBurstID(), 1);

					} else {
						L2Builder::processL2(event);
						//LOG_ERROR("ERROR we should not arrive here!");
					}
				} else { // Event not accepted
					/*
					 * If the Event has been rejected by L1 we can destroy it now
					 */
					//LOG_ERROR("Event: " << event->getEventNumber() <<" discarded from L1");
					EventPool::freeEvent(event);
				}
			} else {
				LOG_ERROR("Bad Level trigger to execute");
				continue;
			}

		} /*else {
		 boost::this_thread::sleep(boost::posix_time::microsec(50));
		 }*/

		//usleep(1000000);
	}
}

void QueueReceiver::onInterruption() {
	running_ = false;
}
}
