/*
 * MonitorConnector.h
 *
 *  Created on: Nov 18, 2011
 *      Author: Jonas Kunze (kunze.jonas@gmail.com)
 */

#pragma once
#ifndef MONITORCONNECTOR_H_
#define MONITORCONNECTOR_H_

#include <boost/asio/deadline_timer.hpp>
#include <boost/asio.hpp>
#include <stdint.h>
#include <cstdbool>
#include <map>
#include <string>
#include <monitoring/IPCHandler.h>


#include <utils/Stopwatch.h>
#include <utils/AExecutable.h>

#define LAST_VALUE_SUFFIX "_lastValue"

namespace na62 {

class EventBuilder;
namespace monitoring {

struct ReceiverRateStruct {
	float bytesPerSecond;
	float packetsPerSecond;
	ReceiverRateStruct() :
			bytesPerSecond(0), packetsPerSecond(0) {
	}
};

class MonitorConnector: public AExecutable {
public:
	MonitorConnector();
	virtual ~MonitorConnector();

	static void setState(STATE state){
		currentState_ = state;
	}

private:
	virtual void thread();
	void onInterruption();
	void handleUpdate();
	uint64_t setDifferentialData(std::string key, uint64_t value);
	uint64_t getDifferentialValue(std::string key);

	void setDetectorDifferentialData(std::string key, uint64_t value,
			uint_fast8_t detectorID);
	void setContinuousData(std::string key, uint64_t value);

	boost::asio::io_service monitoringService;

	boost::asio::deadline_timer timer_;

	Stopwatch updateWatch_;

	std::map<std::string, uint64_t> differentialInts_;
	std::map<uint_fast8_t, std::map<std::string, uint64_t> > detectorDifferentialInts_;

	static STATE currentState_;
};

} /* namespace monitoring */
} /* namespace na62 */
#endif /* MONITORCONNECTOR_H_ */
