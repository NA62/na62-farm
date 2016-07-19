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

namespace na62 {


QueueReceiver::QueueReceiver(){

	running_ = true;
}

QueueReceiver::~QueueReceiver(){}

void QueueReceiver::thread() {
	//All Those variable can became part of the class
	TriggerMessager trigger_message;
	uint priority = 0;

	while (running_) {

		//Receiving Response
		//=================
		if (SharedMemoryManager::popTriggerResponseQueue(trigger_message, priority)) {

			LOG_INFO("Queue Receiver Received event: "<<trigger_message.event_id);

			if (trigger_message.level == 1){

				printf("l0 trigger flags %d \n", trigger_message.l1_trigger_type_word);

				//Fetching the l0 word
				Event* event = EventPool::getEvent(trigger_message.event_id);
				uint_fast8_t l0TriggerTypeWord = event->getL0TriggerTypeWord();
				uint_fast16_t L0L1Trigger(l0TriggerTypeWord | trigger_message.l1_trigger_type_word << 8);

				event->setL1Processed(L0L1Trigger);

				//Removing event from the shared memory
				SharedMemoryManager::removeL1Event(trigger_message.memory_id);

				if (trigger_message.l1_trigger_type_word != 0) {

					if (SourceIDManager::NUMBER_OF_EXPECTED_L1_PACKETS_PER_EVENT != 0) {
						L1Builder::sendL1Request(event);
					} else {
						L2Builder::processL2(event);
					}
				} else { // Event not accepted
					/*
					 * If the Event has been rejected by L1 we can destroy it now
					 */
					EventPool::freeEvent(event);
				}
			} else {
				LOG_INFO("Bad Level trigger to execute");
				continue;
			}

		} else {
			boost::this_thread::sleep(boost::posix_time::microsec(50));
		}

		//usleep(1000000);
	}
}

void QueueReceiver::onInterruption() {
	running_ = false;
}
}
