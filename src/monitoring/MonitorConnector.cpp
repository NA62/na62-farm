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
	timer_.expires_from_now(boost::posix_time::milliseconds(1000));
	timer_.async_wait(boost::bind(&MonitorConnector::handleUpdate, this));

	updateWatch_.reset();

	IPCHandler::updateState(currentState_);

	LOG_INFO<<"Enqueued tasks:\t" << HandleFrameTask::getNumberOfQeuedTasks();

	LOG_INFO<<"IPFragments:\t" << FragmentStore::getNumberOfReceivedFragments()<<"/"<<FragmentStore::getNumberOfReassembledFrames() <<"/"<<FragmentStore::getNumberOfUnfinishedFrames();

	LOG_INFO<<"BurstID:\t" << BurstIdHandler::getCurrentBurstId();
	LOG_INFO<<"NextBurstID:\t" << BurstIdHandler::getNextBurstId();

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
	for (int soruceIDNum = SourceIDManager::NUMBER_OF_L0_DATA_SOURCES - 1;
			soruceIDNum >= 0; soruceIDNum--) {
		uint_fast8_t sourceID = SourceIDManager::sourceNumToID(soruceIDNum);
		statistics << "0x" << std::hex << (int) sourceID << ";";

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

void MonitorConnector::setContinuousData(std::string key, uint64_t value) {
	LOG_INFO<<key << ":\t" << std::to_string(value);
}

}
/* namespace monitoring */
} /* namespace na62 */
