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
#include <boost/lexical_cast.hpp>
#include <glog/logging.h>
#include <iostream>

#include <eventBuilding/SourceIDManager.h>
#include <LKr/L1DistributionHandler.h>
#include <socket/PFringHandler.h>
#include <utils/Utils.h>
#include <monitoring/IPCHandler.h>

#include "../socket/PacketHandler.h"
#include "../eventBuilding/EventBuilder.h"

using namespace boost::interprocess;

namespace na62 {
namespace monitoring {
MonitorConnector::MonitorConnector() :
		timer_(monitoringService) {

	LOG(INFO)<<"Started monitor connector";
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
	LOG(ERROR)<<"Stopping MonitorConnector";
	monitoringService.stop();
}

void MonitorConnector::handleUpdate() {
	// Invoke this method every second
	timer_.expires_from_now(boost::posix_time::milliseconds(1000));
	timer_.async_wait(boost::bind(&MonitorConnector::handleUpdate, this));

	updateWatch_.reset();

	IPCHandler::updateState(RUNNING);

	setDifferentialData("BytesReceived", PFringHandler::GetBytesReceived());
	setDifferentialData("FramesReceived", PFringHandler::GetFramesReceived());

	/*
	 * Number of Events and data rate from all detectors
	 */
	std::stringstream statistics;
	for (int soruceIDNum = SourceIDManager::NUMBER_OF_L0_DATA_SOURCES - 1;
			soruceIDNum >= 0; soruceIDNum--) {
		uint8_t sourceID = SourceIDManager::SourceNumToID(soruceIDNum);
		statistics << "0x" << std::hex << (int) sourceID << ";";

		setDetectorDifferentialData("MEPsReceived",
				PacketHandler::GetMEPsReceivedBySourceID(sourceID), sourceID);
		statistics << std::dec
				<< PacketHandler::GetMEPsReceivedBySourceID(sourceID) << ";";

		setDetectorDifferentialData("EventsReceived",
				PacketHandler::GetEventsReceivedBySourceID(sourceID), sourceID);
		statistics << std::dec
				<< PacketHandler::GetEventsReceivedBySourceID(sourceID) << ";";

		setDetectorDifferentialData("BytesReceived",
				PacketHandler::GetBytesReceivedBySourceID(sourceID), sourceID);
		statistics << std::dec
				<< PacketHandler::GetBytesReceivedBySourceID(sourceID) << ";";
	}

	if (SourceIDManager::NUMBER_OF_EXPECTED_CREAM_PACKETS_PER_EVENT > 0) {
		statistics << "0x" << std::hex << (int) SOURCE_ID_LKr << ";";
	}

	setDetectorDifferentialData("MEPsReceived",
			PacketHandler::GetMEPsReceivedBySourceID(SOURCE_ID_LKr),
			SOURCE_ID_LKr);
	statistics << std::dec
			<< PacketHandler::GetMEPsReceivedBySourceID(SOURCE_ID_LKr) << ";";

	setDetectorDifferentialData("EventsReceived",
			PacketHandler::GetEventsReceivedBySourceID(SOURCE_ID_LKr),
			SOURCE_ID_LKr);
	statistics << std::dec
			<< PacketHandler::GetEventsReceivedBySourceID(SOURCE_ID_LKr) << ";";

	setDetectorDifferentialData("BytesReceived",
			PacketHandler::GetBytesReceivedBySourceID(SOURCE_ID_LKr),
			SOURCE_ID_LKr);
	statistics << std::dec
			<< PacketHandler::GetBytesReceivedBySourceID(SOURCE_ID_LKr) << ";";

	IPCHandler::sendStatistics("DetectorData", statistics.str());

	/*
	 * Trigger word statistics
	 */
	std::stringstream L1Stats;
	std::stringstream L2Stats;
	for (int wordNum = 0x00; wordNum <= 0xFF; wordNum++) {
		std::stringstream stream;
		stream << std::hex << wordNum;

		uint64_t L1Trigs = EventBuilder::GetL1TriggerStats()[wordNum];
		uint64_t L2Trigs = EventBuilder::GetL2TriggerStats()[wordNum];

		setDifferentialData("L1Triggers" + stream.str(), L1Trigs);
		setDifferentialData("L2Triggers" + stream.str(), L2Trigs);

		if (L1Trigs > 0) {
			L1Stats << "0b";
			Utils::bin((uint8_t&) wordNum, L1Stats);
			L1Stats << ";" << L1Trigs << ";";
		}

		if (L2Trigs > 0) {
			L2Stats << "0b";
			Utils::bin((uint8_t&) wordNum, L2Stats);
			L2Stats << ";" << L2Trigs << ";";
		}
	}

	IPCHandler::sendStatistics("L1TriggerData", L1Stats.str());
	IPCHandler::sendStatistics("L2TriggerData", L2Stats.str());

	uint32_t bytesToStorage = EventBuilder::GetBytesSentToStorage();
	uint32_t eventsToStorage = EventBuilder::GetEventsSentToStorage();

	setDifferentialData("BytesToMerger", bytesToStorage);
	setDifferentialData("EventsToMerger", eventsToStorage);

	IPCHandler::sendStatistics("BytesToMerger",
			boost::lexical_cast<std::string>(bytesToStorage));
	IPCHandler::sendStatistics("EventsToMerger",
			boost::lexical_cast<std::string>(eventsToStorage));

	setDifferentialData("L1MRPsSent",
			cream::L1DistributionHandler::GetL1MRPsSent());
	IPCHandler::sendStatistics("L1MRPsSent",
			boost::lexical_cast<std::string>(
					cream::L1DistributionHandler::GetL1MRPsSent()));

	setDifferentialData("L1TriggersSent",
			cream::L1DistributionHandler::GetL1TriggersSent());
	IPCHandler::sendStatistics("L1TriggersSent",
			boost::lexical_cast<std::string>(
					cream::L1DistributionHandler::GetL1TriggersSent()));

	LOG(INFO)<<"BurstID:\t" << EventBuilder::getCurrentBurstId();

	PFringHandler::PrintStats();
}

float MonitorConnector::setDifferentialData(std::string key, uint64_t value) {

	if (differentialInts_.find(key) == differentialInts_.end()) {
		differentialInts_[key + LAST_VALUE_SUFFIX] = 0;
		differentialInts_[key] = 0;
	}
	uint64_t lastValue = differentialInts_[key];

	if (value != 0) {
		LOG(INFO)<<key << ":\t" << boost::lexical_cast<std::string>(value - differentialInts_[key]) << " (" << boost::lexical_cast<std::string>(value) <<")";
	}

	differentialInts_[key + LAST_VALUE_SUFFIX] = differentialInts_[key];
	differentialInts_[key] = value;
	return value - lastValue;
}

void MonitorConnector::setDetectorDifferentialData(std::string key,
		uint64_t value, uint8_t detectorID) {
	uint64_t lastValue;
	if (detectorDifferentialInts_.find(detectorID)
			== detectorDifferentialInts_.end()) {
		detectorDifferentialInts_[detectorID] =
				std::map<std::string, uint64_t>();
		detectorDifferentialInts_[detectorID][key + LAST_VALUE_SUFFIX] = 0;
		detectorDifferentialInts_[detectorID][key] = 0;
	}
	lastValue = detectorDifferentialInts_[detectorID][key];

	LOG(INFO)<<key << boost::lexical_cast<std::string>((int) detectorID) << ":\t" << boost::lexical_cast<std::string>(value - lastValue) << "( " <<boost::lexical_cast<std::string>(value)<<")";

	detectorDifferentialInts_[detectorID][key + LAST_VALUE_SUFFIX] =
			detectorDifferentialInts_[detectorID][key];
	detectorDifferentialInts_[detectorID][key] = value;
}

void MonitorConnector::setContinuousData(std::string key, float value) {
	if (continuousFloats_.find(key) == continuousFloats_.end()
			|| continuousFloats_[key + LAST_VALUE_SUFFIX] != value) {
		LOG(INFO)<<"total " << key + ":\t" + boost::lexical_cast<std::string>(value);
	}
	continuousFloats_[key] = value;
	continuousFloats_[key + LAST_VALUE_SUFFIX] = value;
}

}
/* namespace monitoring */
} /* namespace na62 */
