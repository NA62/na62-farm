/*
 * PoolParser.cpp
 *
 *  Created on: May 16, 2016
 *      Author: marco
 */

#include "PoolParser.h"
#include "structs/TriggerMessager.h"
#include "structs/SerialEvent.h"


#include "../eventBuilding/L2Builder.h"
#include "../eventBuilding/L1Builder.h"

#include <eventBuilding/EventPool.h>
#include <eventBuilding/Event.h>
#include <monitoring/BurstIdHandler.h>

#include <l1/L1TriggerProcessor.h>


#include <tbb/task.h>
#include <tbb/tbb.h>

namespace na62 {

uint PoolParser::highest_burst_id_received_;

PoolParser::PoolParser(){

	running_ = true;
	highest_burst_id_received_= 0;
}

PoolParser::~PoolParser(){}

void PoolParser::thread() {
	uint event_received_per_burst = 0;
	while (running_) {

		static std::atomic<uint> incompleteEvents_;
		incompleteEvents_ = 0;

		static std::atomic<uint> totEvents_;
		totEvents_ = 0;

		static std::atomic<uint> amount_l1_pocessed;
		amount_l1_pocessed = 0;

		static std::atomic<uint> amount_l1_requested;
		amount_l1_requested = 0;

		int index = 0;

		for(size_t index=0; index!= EventPool::getLargestTouchedEventnumberIndex() + 1; index++) {
			Event* event = EventPool::getEventByIndex(index);

			if(event == nullptr) {
				continue;
			}

			totEvents_++;

			if (event->isUnfinished()) {
				LOG_ERROR(" ");
				LOG_ERROR(++index << ") Event:" << event->getEventNumber());
				LOG_ERROR("Is L1 processed: " << event->isL1Processed());
				LOG_ERROR("            " << event->isUnfinished() << " L0 Call counter: " << event->getL0CallCounter());
				LOG_ERROR("Unfinished: " << event->isUnfinished() << " L1 Call counter: " << event->getL1CallCounter());
				++incompleteEvents_;

			}
			if (event->isL1Processed()) {
				amount_l1_pocessed++;
			}
			if (event->isL1Requested()) {
				amount_l1_requested++;
			}
		}

		//if (incompleteEvents_ > 0) {
			LOG_ERROR("   ");
			LOG_ERROR("---FINAL REPORT!---");

			LOG_ERROR("type = PoolParser : burst ID = " << (int) BurstIdHandler::getCurrentBurstId());
			LOG_ERROR("type = PoolParser : l1 processed " << amount_l1_pocessed << "/"<< totEvents_);
			LOG_ERROR("type = PoolParser : l1 requested " << amount_l1_requested << "/"<< totEvents_);
			LOG_ERROR("type = PoolParser : Unfinished: " <<incompleteEvents_ << "/"<< totEvents_);
			LOG_ERROR("   ");

			boost::this_thread::sleep(boost::posix_time::microsec(50));
		//}

		sleep(15);
		//usleep(1000000);
	}
}

void PoolParser::onInterruption() {
	running_ = false;
}
}
