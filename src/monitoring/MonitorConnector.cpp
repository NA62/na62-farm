/*
 * MonitorConnector.cpp
 *
 *  Created on: Nov 18, 2011
 *      Author: Jonas Kunze (kunze.jonas@gmail.com)
 */

#include "MonitorConnector.h"

#include <boost/asio/basic_deadline_timer.hpp>
#include <boost/interprocess/interprocess_fwd.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time_duration.hpp>
#include <boost/date_time/time_duration.hpp>
#include <sstream>

#include "../socket/HandleFrameTask.h"

#include <iostream>

#include <eventBuilding/SourceIDManager.h>
#include <l1/L1DistributionHandler.h>
#include <socket/NetworkHandler.h>
#include <utils/Utils.h>
#include <monitoring/IPCHandler.h>
#include <eventBuilding/Event.h>
#include <eventBuilding/UnfinishedEventsCollector.h>
#include <options/Logging.h>
#include <monitoring/BurstIdHandler.h>

#include <l1/L1TriggerProcessor.h>
#include <l2/L2TriggerProcessor.h>
#include "../eventBuilding/L1Builder.h"
#include "../eventBuilding/L2Builder.h"
#include "../socket/HandleFrameTask.h"
#include "../socket/FragmentStore.h"
#include "../socket/PacketHandler.h"
#include <socket/NetworkHandler.h>
#include <monitoring/HltStatistics.h>

using namespace boost::interprocess;

namespace na62 {
namespace monitoring {

STATE MonitorConnector::currentState_;

MonitorConnector::MonitorConnector() :
		timer_(monitoringService) {

	LOG_INFO("Started monitor connector");
}

void MonitorConnector::thread() {
	timer_.expires_from_now(boost::posix_time::milliseconds(1000));
	timer_.async_wait(boost::bind(&MonitorConnector::handleUpdate, this));
	monitoringService.run();
}

MonitorConnector::~MonitorConnector() {
	monitoringService.stop();
}

void MonitorConnector::onInterruption() {
	LOG_ERROR("Stopping MonitorConnector");
	timer_.cancel();
	monitoringService.stop();
}

void MonitorConnector::handleUpdate() {
	if (!IPCHandler::isRunning()) {
		return;
	}
	// Invoke this method every second
	timer_.expires_from_now(boost::posix_time::milliseconds(1000));
	timer_.async_wait(boost::bind(&MonitorConnector::handleUpdate, this));

	updateWatch_.reset();

	IPCHandler::updateState(currentState_);

	LOG_INFO("Enqueued tasks:\t" << HandleFrameTask::getNumberOfQeuedTasks());
	LOG_INFO(
			"IPFragments:\t" << FragmentStore::getNumberOfReceivedFragments()<<"/"<<FragmentStore::getNumberOfReassembledFrames() <<"/"<<FragmentStore::getNumberOfUnfinishedFrames());
	LOG_INFO("BurstID:\t" << BurstIdHandler::getCurrentBurstId());
	LOG_INFO("State:\t" << currentState_);

	NetworkHandler::PrintStats();

	//singlelongServices
	IPCHandler::sendStatistics("BytesToMerger", std::to_string(L2Builder::GetBytesSentToStorage()));
	IPCHandler::sendStatistics("EventsToMerger", std::to_string(L2Builder::GetEventsSentToStorage()));
	IPCHandler::sendStatistics("L1MRPsSent", std::to_string(l1::L1DistributionHandler::GetL1MRPsSent()));
	IPCHandler::sendStatistics("L1TriggersSent", std::to_string(l1::L1DistributionHandler::GetL1TriggersSent()));
	IPCHandler::sendStatistics("PF_BytesReceived", std::to_string(NetworkHandler::GetBytesReceived()));
	IPCHandler::sendStatistics("PF_PacksReceived", std::to_string(NetworkHandler::GetFramesReceived()));
	IPCHandler::sendStatistics("PF_PacksDropped", std::to_string(NetworkHandler::GetFramesDropped()));

	/*
	 * L1-L2 statistics
	 */
	for (auto& key : HltStatistics::extractKeys()) {
		IPCHandler::sendStatistics(key, std::to_string(HltStatistics::getRollingCounter(key)));
	}

	//
	//multiStatsServices
	//
	/*
	 * Number of Events and data rate from all detectors
	 */
	std::stringstream statistics;
	for (int soruceIDNum = SourceIDManager::NUMBER_OF_L0_DATA_SOURCES - 1; soruceIDNum >= 0; soruceIDNum--) {
		uint_fast8_t sourceID = SourceIDManager::sourceNumToID(soruceIDNum);
		statistics << "0x" << std::hex << (int) sourceID << ";";

		if (SourceIDManager::getExpectedPacksBySourceID(sourceID) > 0) {
			statistics << std::dec
					<< HandleFrameTask::GetMEPsReceivedBySourceNum(soruceIDNum) / SourceIDManager::getExpectedPacksBySourceID(sourceID)
					<< ";";
		}

		statistics << std::dec << Event::getMissingL0EventsBySourceNum(soruceIDNum) << ";";
		statistics << std::dec << HandleFrameTask::GetBytesReceivedBySourceNum(soruceIDNum) << ";";
	}

	if (SourceIDManager::NUMBER_OF_EXPECTED_L1_PACKETS_PER_EVENT != 0) {

		for (int soruceIDNum = SourceIDManager::NUMBER_OF_L1_DATA_SOURCES - 1; soruceIDNum >= 0; soruceIDNum--) {

			uint_fast8_t sourceID = SourceIDManager::l1SourceNumToID(soruceIDNum);
			statistics << "0x" << std::hex << (int) sourceID << ";";

			if (SourceIDManager::getExpectedL1PacksBySourceID(sourceID) > 0) {
				statistics << std::dec
						<< HandleFrameTask::GetL1MEPsReceivedBySourceNum(soruceIDNum)
								/ SourceIDManager::getExpectedL1PacksBySourceID(sourceID) << ";";
			}

			statistics << std::dec << Event::getMissingL1EventsBySourceNum(soruceIDNum) << ";";
			statistics << std::dec << HandleFrameTask::GetL1BytesReceivedBySourceNum(soruceIDNum) << ";";
		}
	}
	IPCHandler::sendStatistics("DetectorData", statistics.str());

	for (auto& key : HltStatistics::extractDimensionalKeys()) {
		IPCHandler::sendStatistics(key, HltStatistics::serializeDimensionalCounter(key));
	}

	/*
	 * Trigger word statistics
	 */
	std::stringstream L1Stats;
	std::stringstream L2Stats;
	for (int wordNum = 0x00; wordNum <= 0xFF; wordNum++) {
		uint64_t L1Trigs = HltStatistics::getL1TriggerStats()[wordNum];
		uint64_t L2Trigs = HltStatistics::getL2TriggerStats()[wordNum];

		if (L1Trigs > 0) {
			L1Stats << "0b";
			Utils::bin((uint_fast8_t&) wordNum, L1Stats);
			L1Stats << ";" << L1Trigs << ";";
		}

		if (L2Trigs > 0) {
			L2Stats << "0b";
			Utils::bin((uint_fast8_t&) wordNum, L2Stats);
			L2Stats << ";" << L2Trigs << ";";
		}
	}

	IPCHandler::sendStatistics("L1TriggerData", L1Stats.str());
	IPCHandler::sendStatistics("L2TriggerData", L2Stats.str());

	/*
	 * Building Time L0-L1 statistics
	 *
	 */
	uint64_t L0BuildTimeMean = 0;
	uint64_t L1BuildTimeMean = 0;
	uint64_t L1InputEventsPerBurst = L1TriggerProcessor::GetL1InputEventsPerBurst();
	uint64_t L2InputEventsPerBurst = L2TriggerProcessor::GetL2InputEventsPerBurst();

	if (L1Builder::GetL0BuildingTimeCumulative()) {
		if (L1InputEventsPerBurst) {
			L0BuildTimeMean = L1Builder::GetL0BuildingTimeCumulative() / L1InputEventsPerBurst;
		}
	}
	if (L2Builder::GetL1BuildingTimeCumulative()) {
		if (L2InputEventsPerBurst) {
			L1BuildTimeMean = L2Builder::GetL1BuildingTimeCumulative() / L2InputEventsPerBurst;
		}
	}
	uint64_t L0BuildTimeMax = L1Builder::GetL0BuildingTimeMax();
	uint64_t L1BuildTimeMax = L2Builder::GetL1BuildingTimeMax();

	//singlelongServices
	IPCHandler::sendStatistics("L0BuildingTimeMean", std::to_string(L0BuildTimeMean));
	IPCHandler::sendStatistics("L1BuildingTimeMean", std::to_string(L1BuildTimeMean));
	IPCHandler::sendStatistics("L0BuildingTimeMax", std::to_string(L0BuildTimeMax));
	IPCHandler::sendStatistics("L1BuildingTimeMax", std::to_string(L1BuildTimeMax));

	/*
	 * Timing L1-L2 statistics
	 *
	 */
	uint64_t L1ProcTimeMean = 0;
	uint64_t L2ProcTimeMean = 0;

	if (L1Builder::GetL1ProcessingTimeCumulative()) {
		if (L1InputEventsPerBurst) {
			L1ProcTimeMean = L1Builder::GetL1ProcessingTimeCumulative() / L1InputEventsPerBurst;
		}
	}
	if (L2Builder::GetL2ProcessingTimeCumulative()) {
		if (L2InputEventsPerBurst) {
			L2ProcTimeMean = L2Builder::GetL2ProcessingTimeCumulative() / L2InputEventsPerBurst;
		}
	}

	uint64_t L1ProcTimeMax = L1Builder::GetL1ProcessingTimeMax();
	uint64_t L2ProcTimeMax = L2Builder::GetL2ProcessingTimeMax();

	//singlelongServices
	IPCHandler::sendStatistics("L1ProcessingTimeMean", std::to_string(L1ProcTimeMean));
	IPCHandler::sendStatistics("L2ProcessingTimeMean", std::to_string(L2ProcTimeMean));
	IPCHandler::sendStatistics("L1ProcessingTimeMax", std::to_string(L1ProcTimeMax));
	IPCHandler::sendStatistics("L2ProcessingTimeMax", std::to_string(L2ProcTimeMax));

	/*
	 * Timing statistics for histograms
	 */
	std::stringstream L0BuildTimeVsEvtNumStats;
	std::stringstream L1BuildTimeVsEvtNumStats;
	std::stringstream L1ProcTimeVsEvtNumStats;
	std::stringstream L2ProcTimeVsEvtNumStats;

	for (int timeId = 0x00; timeId < 0x64 + 1; timeId++) {
		for (int tsId = 0x00; tsId < 0x32 + 1; tsId++) {

			uint64_t L0BuildTimeVsEvtNum = L1Builder::GetL0BuidingTimeVsEvtNumber()[timeId][tsId];
			uint64_t L1BuildTimeVsEvtNum = L2Builder::GetL1BuidingTimeVsEvtNumber()[timeId][tsId];
			uint64_t L1ProcTimeVsEvtNum = L1Builder::GetL1ProcessingTimeVsEvtNumber()[timeId][tsId];
			uint64_t L2ProcTimeVsEvtNum = L2Builder::GetL2ProcessingTimeVsEvtNumber()[timeId][tsId];

			if (L0BuildTimeVsEvtNum > 0) {
				L0BuildTimeVsEvtNumStats << timeId << "," << tsId << "," << L0BuildTimeVsEvtNum << ";";
			}
			if (L1BuildTimeVsEvtNum > 0) {
				L1BuildTimeVsEvtNumStats << timeId << "," << tsId << "," << L1BuildTimeVsEvtNum << ";";
			}
			if (L1ProcTimeVsEvtNum > 0) {
				L1ProcTimeVsEvtNumStats << timeId << "," << tsId << "," << L1ProcTimeVsEvtNum << ";";
			}
			if (L2ProcTimeVsEvtNum > 0) {
				L2ProcTimeVsEvtNumStats << timeId << "," << tsId << "," << L2ProcTimeVsEvtNum << ";";
			}
		}
	}

	IPCHandler::sendStatistics("L0BuildingTimeVsEvtNumber", L0BuildTimeVsEvtNumStats.str());
	IPCHandler::sendStatistics("L1BuildingTimeVsEvtNumber", L1BuildTimeVsEvtNumStats.str());
	IPCHandler::sendStatistics("L1ProcessingTimeVsEvtNumber", L1ProcTimeVsEvtNumStats.str());
	IPCHandler::sendStatistics("L2ProcessingTimeVsEvtNumber", L2ProcTimeVsEvtNumStats.str());
}

}
/* namespace monitoring */
} /* namespace na62 */
