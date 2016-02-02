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
#include <LKr/L1DistributionHandler.h>
#include <socket/NetworkHandler.h>
#include <utils/Utils.h>
#include <monitoring/IPCHandler.h>
#include <eventBuilding/Event.h>
#include <eventBuilding/UnfinishedEventsCollector.h>
#include <options/Logging.h>
#include <monitoring/BurstIdHandler.h>

#include "../eventBuilding/L1Builder.h"
#include "../eventBuilding/L2Builder.h"
#include "../socket/HandleFrameTask.h"
#include "../socket/FragmentStore.h"
#include "../socket/PacketHandler.h"
#include <socket/NetworkHandler.h>

using namespace boost::interprocess;

namespace na62 {
namespace monitoring {

STATE MonitorConnector::currentState_;

MonitorConnector::MonitorConnector() :
		timer_(monitoringService) {

	LOG_INFO<<"Started monitor connector";
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
	LOG_ERROR<< "Stopping MonitorConnector" << ENDL;
	timer_.cancel();
	monitoringService.stop();
}

void MonitorConnector::handleUpdate() {
	if (!IPCHandler::isRunning()) {
		return;
	}
	// Invoke this method every second
	timer_.expires_from_now(boost::posix_time::milliseconds(10000));
	timer_.async_wait(boost::bind(&MonitorConnector::handleUpdate, this));

	// GLM TEST: Change burst ID
	//int bid = BurstIdHandler::getCurrentBurstId()+1;
	//BurstIdHandler::setNextBurstID(bid);

	updateWatch_.reset();

	IPCHandler::updateState(currentState_);

	LOG_INFO<<"Enqueued tasks:\t" << HandleFrameTask::getNumberOfQeuedTasks();

	LOG_INFO<<"IPFragments:\t" << FragmentStore::getNumberOfReceivedFragments()<<"/"<<FragmentStore::getNumberOfReassembledFrames() <<"/"<<FragmentStore::getNumberOfUnfinishedFrames();

	LOG_INFO<<"BurstID:\t" << BurstIdHandler::getCurrentBurstId();
	//LOG_INFO<<"NextBurstID:\t" << BurstIdHandler::getNextBurstId();

	LOG_INFO<<"State:\t" << currentState_;

	setDifferentialData("Sleeps", PacketHandler::sleeps_);
	setDifferentialData("Spins", PacketHandler::spins_);
	setContinuousData("SendTimer",
			PacketHandler::sendTimer.elapsed().wall / 1000);
	setDifferentialData("SpawnedTasks",
			PacketHandler::frameHandleTasksSpawned_);
	setContinuousData("AggregationSize",
			NetworkHandler::GetFramesReceived()
					/ (float) PacketHandler::frameHandleTasksSpawned_);

	NetworkHandler::PrintStats();

	IPCHandler::sendStatistics("PF_BytesReceived",
			std::to_string(NetworkHandler::GetBytesReceived()));
	IPCHandler::sendStatistics("PF_PacksReceived",
			std::to_string(NetworkHandler::GetFramesReceived()));
	IPCHandler::sendStatistics("PF_PacksDropped",
			std::to_string(NetworkHandler::GetFramesDropped()));

	LOG_INFO<<"########################";
	/*
	 * Number of Events and data rate from all detectors
	 */
	std::stringstream statistics;
//	std::stringstream monitoring;
	for (int soruceIDNum = SourceIDManager::NUMBER_OF_L0_DATA_SOURCES - 1;
			soruceIDNum >= 0; soruceIDNum--) {
		uint_fast8_t sourceID = SourceIDManager::sourceNumToID(soruceIDNum);
		statistics << "0x" << std::hex << (int) sourceID << ";";
//		monitoring << "0x" << std::hex << (int) sourceID << std::dec;

		if (SourceIDManager::getExpectedPacksBySourceID(sourceID) > 0) {
			setDetectorDifferentialData("MEPsReceived",
					HandleFrameTask::GetMEPsReceivedBySourceNum(soruceIDNum)
							/ SourceIDManager::getExpectedPacksBySourceID(
									sourceID), sourceID);

			statistics << std::dec
					<< HandleFrameTask::GetMEPsReceivedBySourceNum(soruceIDNum)
							/ SourceIDManager::getExpectedPacksBySourceID(
									sourceID) << ";";
		}

		setDetectorDifferentialData("EventsReceived",
				Event::getMissingEventsBySourceNum(soruceIDNum), sourceID);
		statistics << std::dec
				<< Event::getMissingEventsBySourceNum(soruceIDNum) << ";";

//		setDetectorDifferentialData("BytesReceived",
//				HandleFrameTask::GetBytesReceivedBySourceNum(soruceIDNum),
//				sourceID);
		statistics << std::dec
				<< HandleFrameTask::GetBytesReceivedBySourceNum(soruceIDNum)
				<< ";";
//		for (uint f = 0;
//				f != SourceIDManager::getExpectedPacksBySourceNum(soruceIDNum);
//				f++) {
//			uint_fast8_t subID = (uint_fast8_t) f;
//			monitoring << ":" << f << ","
//					<< Event::getReceivedEventsBySourceNumBySubId(soruceIDNum,f);
//			if (f== SourceIDManager::getExpectedPacksBySourceNum(soruceIDNum)- 1)
//				monitoring << ";" << ENDL;
//			setDetectorSubIdDifferentialData("EventsReceivedBySubId-",
//					Event::getReceivedEventsBySourceNumBySubId(soruceIDNum, f),
//					subID, sourceID);
//		}
	}

	if (SourceIDManager::NUMBER_OF_EXPECTED_CREAM_PACKETS_PER_EVENT != 0) {
		statistics << "0x" << std::hex << (int) SOURCE_ID_LKr << ";";

		/*
		 * Store CREAM specific statistics stored at SourceIDManager::NUMBER_OF_L0_DATA_SOURCES as sourceNum
		 */
		setDetectorDifferentialData("MEPsReceived",
				HandleFrameTask::GetMEPsReceivedBySourceNum(
						SourceIDManager::NUMBER_OF_L0_DATA_SOURCES)
						/ (SourceIDManager::NUMBER_OF_EXPECTED_CREAM_PACKETS_PER_EVENT),
				SOURCE_ID_LKr);
		statistics << std::dec
				<< HandleFrameTask::GetMEPsReceivedBySourceNum(
						SourceIDManager::NUMBER_OF_L0_DATA_SOURCES)
						/ (SourceIDManager::NUMBER_OF_EXPECTED_CREAM_PACKETS_PER_EVENT)
				<< ";";

		setDetectorDifferentialData("EventsReceived",
				Event::getMissingEventsBySourceNum(
						SourceIDManager::NUMBER_OF_L0_DATA_SOURCES),
				SOURCE_ID_LKr);

		statistics << std::dec
				<< Event::getMissingEventsBySourceNum(
						SourceIDManager::NUMBER_OF_L0_DATA_SOURCES) << ";";

//		setDetectorDifferentialData("BytesReceived",
//				HandleFrameTask::GetBytesReceivedBySourceNum(
//						SourceIDManager::NUMBER_OF_L0_DATA_SOURCES),
//				SOURCE_ID_LKr);
		statistics << std::dec
				<< HandleFrameTask::GetBytesReceivedBySourceNum(
						SourceIDManager::NUMBER_OF_L0_DATA_SOURCES) << ";";

		setDetectorDifferentialData("NonRequestedCreamFrags",
				Event::getNumberOfNonRequestedCreamFragments(),
				SOURCE_ID_LKr);
	}

	IPCHandler::sendStatistics("DetectorData", statistics.str());
//	IPCHandler::sendStatistics("MonitoringData", monitoring.str());
//	LOG_INFO<< monitoring.str() << ENDL;

	uint_fast32_t L1InputEvents = L1Builder::GetL1InputStats();
	setDifferentialData("L1InputEvents ", L1InputEvents);
	IPCHandler::sendStatistics("L1InputEvents", std::to_string(L1InputEvents));

	uint_fast32_t L2InputEvents = L2Builder::GetL2InputStats();
	setDifferentialData("L2InputEvents ", L2InputEvents);
	IPCHandler::sendStatistics("L2InputEvents", std::to_string(L2InputEvents));

	uint_fast32_t L1RequestToCreams = L1Builder::GetL1RequestToCreams();
	setDifferentialData("L1RequestToCreams ", L1RequestToCreams);
	IPCHandler::sendStatistics("L1RequestToCreams",
			std::to_string(L1RequestToCreams));

	uint_fast32_t L1BypassedEvents = L1Builder::GetL1BypassedEvents();
	setDifferentialData("L1BypassedEvents ", L1BypassedEvents);
	IPCHandler::sendStatistics("L1BypassedEvents",
			std::to_string(L1BypassedEvents));

	/*
	 * Trigger word statistics
	 */
	std::stringstream L1Stats;
	std::stringstream L2Stats;
	for (int wordNum = 0x00; wordNum <= 0xFF; wordNum++) {
		std::stringstream stream;
		stream << std::hex << wordNum;

		uint64_t L1Trigs = L1Builder::GetL1TriggerStats()[wordNum];
		uint64_t L2Trigs = L2Builder::GetL2TriggerStats()[wordNum];

		setDifferentialData("L1Triggers" + stream.str(), L1Trigs);
		setDifferentialData("L2Triggers" + stream.str(), L2Trigs);

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

	LOG_INFO<<"########################";

	setDifferentialData("BytesReceived", NetworkHandler::GetBytesReceived());
	setDifferentialData("FramesReceived", NetworkHandler::GetFramesReceived());
	if (getDifferentialValue("FramesReceived") != 0) {
		setContinuousData("FrameSize",
				getDifferentialValue("BytesReceived")
						/ getDifferentialValue("FramesReceived"));
	}

	IPCHandler::sendStatistics("L1TriggerData", L1Stats.str());
	IPCHandler::sendStatistics("L2TriggerData", L2Stats.str());
	uint_fast32_t bytesToStorage = L2Builder::GetBytesSentToStorage();
	uint_fast32_t eventsToStorage = L2Builder::GetEventsSentToStorage();

	setDifferentialData("BytesToMerger", bytesToStorage);
	setDifferentialData("EventsToMerger", eventsToStorage);

	IPCHandler::sendStatistics("BytesToMerger", std::to_string(bytesToStorage));
	IPCHandler::sendStatistics("EventsToMerger",
			std::to_string(eventsToStorage));

	setDifferentialData("L1MRPsSent",
			cream::L1DistributionHandler::GetL1MRPsSent());
	IPCHandler::sendStatistics("L1MRPsSent",
			std::to_string(cream::L1DistributionHandler::GetL1MRPsSent()));

	setDifferentialData("L1TriggersSent",
			cream::L1DistributionHandler::GetL1TriggersSent());
	IPCHandler::sendStatistics("L1TriggersSent",
			std::to_string(cream::L1DistributionHandler::GetL1TriggersSent()));

	setDifferentialData("FramesSent", NetworkHandler::GetFramesSent());
	setContinuousData("OutFramesQueued",
			NetworkHandler::getNumberOfEnqueuedSendFrames());

	LOG_INFO<<"IPFragments:\t" << FragmentStore::getNumberOfReceivedFragments()<<"/"<<FragmentStore::getNumberOfReassembledFrames() <<"/"<<FragmentStore::getNumberOfUnfinishedFrames();
	LOG_INFO<<"=======================================";

	/*
	 * Building Time L0-L1 statistics
	 *
	 */
	uint64_t L0BuildTimeMean = 0;
	uint64_t L1BuildTimeMean = 0;

	if (L1Builder::GetL0BuildingTimeCumulative()) {
//		LOG_INFO<< "***********L0BuildingTimeCumulative " << L1Builder::GetL0BuildingTimeCumulative() << ENDL;
//		LOG_INFO<< "***********L1InputEventsPerBurst " << L1Builder::GetL1InputEventsPerBurst() << ENDL;
		L0BuildTimeMean = L1Builder::GetL0BuildingTimeCumulative()/L1Builder::GetL1InputEventsPerBurst();
	}
	if (L2Builder::GetL1BuildingTimeCumulative()) {
//		LOG_INFO<< "***********L1BuildingTimeCumulative " << L2Builder::GetL1BuildingTimeCumulative() << ENDL;
//		LOG_INFO<< "***********L2InputEventsPerBurst " << L2Builder::GetL2InputEventsPerBurst() << ENDL;
		L1BuildTimeMean = L2Builder::GetL1BuildingTimeCumulative()/L2Builder::GetL2InputEventsPerBurst();
	}
//	LOG_INFO<< "***********L0BuildingTimeMax " << L1Builder::GetL0BuildingTimeMax() << ENDL;
	uint64_t L0BuildTimeMax = L1Builder::GetL0BuildingTimeMax();

//	LOG_INFO<< "***********L1BuildingTimeMax " << L2Builder::GetL1BuildingTimeMax() << ENDL;
	uint64_t L1BuildTimeMax = L2Builder::GetL1BuildingTimeMax();

	LOG_INFO<< "***********L0BuildTimeMean (x Run Control) " << L0BuildTimeMean << ENDL;
	LOG_INFO<< "***********L1BuildTimeMean (x Run Control) " << L1BuildTimeMean << ENDL;
	LOG_INFO<< "***********L0BuildTimeMax  (x Run Control) " << L0BuildTimeMax << ENDL;
	LOG_INFO<< "***********L1BuildTimeMax  (x Run Control) " << L1BuildTimeMax << ENDL;

	IPCHandler::sendStatistics("L0BuildingTimeMean",
			std::to_string(L0BuildTimeMean));
	IPCHandler::sendStatistics("L1BuildingTimeMean",
			std::to_string(L1BuildTimeMean));
	IPCHandler::sendStatistics("L0BuildingTimeMax",
			std::to_string(L0BuildTimeMax));
	IPCHandler::sendStatistics("L1BuildingTimeMax",
			std::to_string(L1BuildTimeMax));
	/*
	 * Timing L1-L2 statistics
	 *
	 */
	uint64_t L1ProcTimeMean = 0;
	uint64_t L2ProcTimeMean = 0;

	if (L1Builder::GetL1ProcessingTimeCumulative()) {
//		LOG_INFO<< "***********L1ProcessingTimeCumulative " << L1Builder::GetL1ProcessingTimeCumulative() << ENDL;
//		LOG_INFO<< "***********L1InputEventsPerBurst " << L1Builder::GetL1InputEventsPerBurst() << ENDL;
		L1ProcTimeMean = L1Builder::GetL1ProcessingTimeCumulative()/L1Builder::GetL1InputEventsPerBurst();
	}
	if (L2Builder::GetL2ProcessingTimeCumulative()) {
//		LOG_INFO<< "***********L2ProcessingTimeCumulative " << L2Builder::GetL2ProcessingTimeCumulative() << ENDL;
//		LOG_INFO<< "***********L2InputEventsPerBurst " << L2Builder::GetL2InputEventsPerBurst() << ENDL;
		if(L2Builder::GetL2InputEventsPerBurst())
			L2ProcTimeMean = L2Builder::GetL2ProcessingTimeCumulative()/L2Builder::GetL2InputEventsPerBurst();
		else
			L2ProcTimeMean = -1;
	}

//	LOG_INFO<< "***********L1ProcessingTimeMax " << L1Builder::GetL1ProcessingTimeMax() << ENDL;
	uint64_t L1ProcTimeMax = L1Builder::GetL1ProcessingTimeMax();

//	LOG_INFO<< "***********L2ProcessingTimeMax " << L2Builder::GetL2ProcessingTimeMax() << ENDL;
	uint64_t L2ProcTimeMax = L2Builder::GetL2ProcessingTimeMax();

	LOG_INFO<< "***********L1ProcTimeMean (x Run Control)  " << L1ProcTimeMean << ENDL;
	LOG_INFO<< "***********L2ProcTimeMean (x Run Control)  " << L2ProcTimeMean << ENDL;
	LOG_INFO<< "***********L1ProcTimeMax  (x Run Control)  " << L1ProcTimeMax << ENDL;
	LOG_INFO<< "***********L2ProcTimeMax  (x Run Control)  " << L2ProcTimeMax << ENDL;

	IPCHandler::sendStatistics("L1ProcessingTimeMean",
			std::to_string(L1ProcTimeMean));
	IPCHandler::sendStatistics("L2ProcessingTimeMean",
			std::to_string(L2ProcTimeMean));
	IPCHandler::sendStatistics("L1ProcessingTimeMax",
			std::to_string(L1ProcTimeMax));
	IPCHandler::sendStatistics("L2ProcessingTimeMax",
			std::to_string(L2ProcTimeMax));

	/*
	 * Timing statistics for histograms
	 */
	std::stringstream L0BuildTimeVsEvtNumStats;
	std::stringstream L1BuildTimeVsEvtNumStats;
	std::stringstream L1ProcTimeVsEvtNumStats;
	std::stringstream L2ProcTimeVsEvtNumStats;

	for (int timeId = 0x00; timeId < 0x64 + 1; timeId++) {
		for (int tsId = 0x00; tsId < 0x32 + 1; tsId++) {

			uint64_t L0BuildTimeVsEvtNum =
					L1Builder::GetL0BuidingTimeVsEvtNumber()[timeId][tsId];
			uint64_t L1BuildTimeVsEvtNum =
					L2Builder::GetL1BuidingTimeVsEvtNumber()[timeId][tsId];
			uint64_t L1ProcTimeVsEvtNum =
					L1Builder::GetL1ProcessingTimeVsEvtNumber()[timeId][tsId];
			uint64_t L2ProcTimeVsEvtNum =
					L2Builder::GetL2ProcessingTimeVsEvtNumber()[timeId][tsId];
//					NetworkHandler::GetPacketTimeDiffVsTime()[timeId][tsId];

			if (L0BuildTimeVsEvtNum > 0) {
				L0BuildTimeVsEvtNumStats << timeId << "," << tsId << ","
						<< L0BuildTimeVsEvtNum << ";";
			}
			if (L1BuildTimeVsEvtNum > 0) {
				L1BuildTimeVsEvtNumStats << timeId << "," << tsId << ","
						<< L1BuildTimeVsEvtNum << ";";
			}
			if (L1ProcTimeVsEvtNum > 0) {
				L1ProcTimeVsEvtNumStats << timeId << "," << tsId << ","
						<< L1ProcTimeVsEvtNum << ";";
			}
			if (L2ProcTimeVsEvtNum > 0) {
				L2ProcTimeVsEvtNumStats << timeId << "," << tsId << ","
						<< L2ProcTimeVsEvtNum << ";";
			}
		}
	}
//	LOG_INFO<<"########################" << L0BuildTimeVsEvtNumStats.str() << ENDL;
//	LOG_INFO<<"########################" << L1BuildTimeVsEvtNumStats.str() << ENDL;
//	LOG_INFO<<"########################" << L1ProcTimeVsEvtNumStats.str() << ENDL;
//	LOG_INFO<<"########################" << L2ProcTimeVsEvtNumStats.str() << ENDL;

	IPCHandler::sendStatistics("L0BuildingTimeVsEvtNumber",L0BuildTimeVsEvtNumStats.str());
	IPCHandler::sendStatistics("L1BuildingTimeVsEvtNumber",L1BuildTimeVsEvtNumStats.str());
	IPCHandler::sendStatistics("L1ProcessingTimeVsEvtNumber",L1ProcTimeVsEvtNumStats.str());
	IPCHandler::sendStatistics("L2ProcessingTimeVsEvtNumber",L2ProcTimeVsEvtNumStats.str());

	IPCHandler::sendStatistics("UnfinishedEventsData",
			UnfinishedEventsCollector::toJson());
}

uint64_t MonitorConnector::setDifferentialData(std::string key,
		uint64_t value) {

	if (differentialInts_.find(key) == differentialInts_.end()) {
		differentialInts_[key + LAST_VALUE_SUFFIX] = 0;
		differentialInts_[key] = 0;
	}
	uint64_t lastValue = differentialInts_[key];

	if (value != 0) {
		if (key == "BytesReceived") {
			LOG_INFO<<key << ":\t" << Utils::FormatSize(value - differentialInts_[key]) << " (" << Utils::FormatSize(value) <<")";
		} else {
			LOG_INFO<<key << ":\t" << std::to_string(value - differentialInts_[key]) << " (" << std::to_string(value) <<")";
		}

	}

	differentialInts_[key + LAST_VALUE_SUFFIX] = differentialInts_[key];
	differentialInts_[key] = value;
	return value - lastValue;
}

uint64_t MonitorConnector::getDifferentialValue(std::string key) {
	if (differentialInts_.find(key) != differentialInts_.end()) {
		return differentialInts_[key]
				- differentialInts_[key + LAST_VALUE_SUFFIX];
	}
	return 0;
}

void MonitorConnector::setDetectorDifferentialData(std::string key,
		uint64_t value, uint_fast8_t detectorID) {
	uint64_t lastValue;
	if (detectorDifferentialInts_.find(detectorID)
			== detectorDifferentialInts_.end()) {
		detectorDifferentialInts_[detectorID] =
				std::map<std::string, uint64_t>();
		detectorDifferentialInts_[detectorID][key + LAST_VALUE_SUFFIX] = 0;
		detectorDifferentialInts_[detectorID][key] = 0;
	}
	lastValue = detectorDifferentialInts_[detectorID][key];

	LOG_INFO<<key << SourceIDManager::sourceIdToDetectorName(detectorID) << ":\t" << std::to_string(value - lastValue) << "( " <<std::to_string(value)<<")";

	detectorDifferentialInts_[detectorID][key + LAST_VALUE_SUFFIX] =
			detectorDifferentialInts_[detectorID][key];
	detectorDifferentialInts_[detectorID][key] = value;
}

//void MonitorConnector::setDetectorSubIdDifferentialData(std::string key,
//		uint64_t value, uint_fast8_t subID, uint_fast8_t detectorID) {
//	uint64_t lastValue;
//	if (detectorSubIdDifferentialInts_.find(detectorID)
//			== detectorSubIdDifferentialInts_.end()) {
//		detectorSubIdDifferentialInts_[detectorID] = std::map<uint_fast8_t,
//				std::map<std::string, uint64_t>>();
//		if (detectorSubIdDifferentialInts_[detectorID].find(subID)
//				== detectorSubIdDifferentialInts_[detectorID].end()) {
//			detectorSubIdDifferentialInts_[detectorID][subID][key
//					+ LAST_VALUE_SUFFIX] = 0;
//			detectorSubIdDifferentialInts_[detectorID][subID][key] = 0;
//		}
//	}
//	lastValue = detectorSubIdDifferentialInts_[detectorID][subID][key];
//
//	LOG_INFO<<key << SourceIDManager::sourceIdToDetectorName(detectorID) << ", subID "<< (uint)subID << ":\t" << std::to_string(value - lastValue) << "( " <<std::to_string(value)<<")";
//
//	detectorSubIdDifferentialInts_[detectorID][subID][key + LAST_VALUE_SUFFIX] =
//			detectorSubIdDifferentialInts_[detectorID][subID][key];
//	detectorSubIdDifferentialInts_[detectorID][subID][key] = value;
//}

void MonitorConnector::setContinuousData(std::string key, uint64_t value) {
	LOG_INFO<<key << ":\t" << std::to_string(value);
}

}
/* namespace monitoring */
} /* namespace na62 */
