/*
 * MyOptions.h
 *
 *  Created on: Apr 11, 2014
 \*      Author: Jonas Kunze (kunze.jonas@gmail.com)
 */

#pragma once
#ifndef MYOPTIONS_H_
#define MYOPTIONS_H_

#include <options/Options.h>
#include <string>
#include <boost/thread.hpp>

/*
 * Listening Ports
 */
#define OPTION_ETH_DEVICE_NAME (char*)"ethDeviceName"

#define OPTION_L0_RECEIVER_PORT (char*)"L0Port"
#define OPTION_CREAM_RECEIVER_PORT (char*)"CREAMPort"

/*
 * Event Building
 */
#define OPTION_NUMBER_OF_EBS (char*)"numberOfEB"
#define OPTION_DATA_SOURCE_IDS (char*)"L0DataSourceIDs"

#define OPTION_TS_SOURCEID (char*)"timestampSourceID"

#define OPTION_CREAM_CRATES (char*)"CREAMCrates"
#define OPTION_INACTIVE_CREAM_CRATES (char*)"inactiveCREAMCrates"

#define OPTION_FIRST_BURST_ID (char*)"firstBurstID"

#define OPTION_CREAM_MULTICAST_GROUP (char*)"creamMulticastIP"
#define OPTION_CREAM_MULTICAST_PORT (char*)"creamMulticastPort"
#define OPTION_MAX_TRIGGERS_PER_L1MRP (char*)"maxTriggerPerL1MRP"

#define OPTION_MAX_NUMBER_OF_EVENTS_PER_BURST (char*)"maxNumberOfEventsPerBurst"

#define OPTION_SEND_MRP_WITH_ZSUPPRESSION_FLAG (char*)"sendMRPsWithZSuppressionFlag"

#define OPTION_INCREMENT_BURST_AT_EOB (char*)"incrementBurstAtEOB"

#define OPTION_L1_FLAG_MODE (char*) "L1FlagMode"
/*
 * Triggering
 */
#define OPTION_L1_REDUCTION_FACTOR  (char*)"L1ReductionFactor"
#define OPTION_L2_REDUCTION_FACTOR  (char*)"L2ReductionFactor"

#define OPTION_L1_DOWNSCALE_FACTOR  (char*)"L1DownscaleFactor"
#define OPTION_L2_DOWNSCALE_FACTOR  (char*)"L2DownscaleFactor"

#define OPTION_MIN_USEC_BETWEEN_L1_REQUESTS (char*)"minUsecsBetweenL1Requests"

/*
 * Merger
 */
#define OPTION_MERGER_HOST_NAMES (char*)"mergerHostNames"
#define OPTION_MERGER_PORT (char*)"mergerPort"

/*
 * Performance
 */
#define OPTION_PH_SCHEDULER (char*) "packetHandlerScheduler"
#define OPTION_ZMQ_IO_THREADS (char*)"zmqIoThreads"
#define OPTION_ACTIVE_POLLING (char*)"activePolling"
#define OPTION_POLLING_DELAY (char*)"pollingDelay"
#define OPTION_POLLING_SLEEP_MICROS (char*)"pollingSleepMicros"
#define OPTION_MAX_FRAME_AGGREGATION (char*)"maxFramesAggregation"
#define OPTION_MAX_AGGREGATION_TIME (char*)"maxAggregationTime"

/*
 * MUVs
 */
#define OPTION_MUV_CREAM_CRATE_ID (char*)"muvCreamCrateID"

/*
 *  STRAW
 */
#define OPTION_STRAW_PORT (char*)"strawReceivePort"
#define OPTION_STRAW_ZMQ_PORT (char*)"strawZmqPort"
#define OPTION_STRAW_ZMQ_DST_HOSTS (char*)"strawZmqDstHosts"

/*
 * Debugging
 */
#define OPTION_PRINT_MISSING_SOURCES (char*)"printMissingSources"
#define OPTION_WRITE_BROKEN_CREAM_INFO (char*)"printBrokenCreamInfo"

namespace na62 {
class MyOptions: public Options {
public:
	MyOptions();
	virtual ~MyOptions();

	static void Load(int argc, char* argv[]) {
		desc.add_options()

		(OPTION_CONFIG_FILE,
				po::value<std::string>()->default_value("/etc/na62-farm.cfg"),
				"Config file for the options shown here")

		(OPTION_ETH_DEVICE_NAME,
				po::value<std::string>()->default_value("dna0"),
				"Name of the device to be used for receiving data")

		(OPTION_L0_RECEIVER_PORT, po::value<int>()->default_value(58913),
				"UDP-Port for L1 data reception")

		(OPTION_NUMBER_OF_EBS,
				po::value<int>()->default_value(
						boost::thread::hardware_concurrency() - 4),
				"Number of threads to be used for eventbuilding and L1/L2 processing")

		(OPTION_DATA_SOURCE_IDS, po::value<std::string>()->required(),
				"Comma separated list of all available data source IDs sending Data to L1 (all but LKr) together with the expected numbers of packets per source. The format is like following (A,B,C are sourceIDs and a,b,c are the number of expected packets per source):\n \t A:a,B:b,C:c")

		(OPTION_CREAM_RECEIVER_PORT, po::value<int>()->default_value(58915),
				"UDP-Port for L2 CREAM data reception")

		(OPTION_CREAM_CRATES, po::value<std::string>()->required(),
				"Defines the expected sourceIDs within the data packets from the CREAMs. The format is $crateID1:$CREAMIDs,$crateID1:$CREAMIDs,$crateID2:$CREAMIDs... E.g. 1:2-4,1:11-13,2:2-5,2:7 for two crates (1 and 2) with following IDs (2,3,4,11,12,13 and 2,3,4,5,7).")

		(OPTION_INACTIVE_CREAM_CRATES,
				po::value<std::string>()->default_value(""),
				"Defines a list of CREAMs that must appear in the normal creamCrate list but should not be activated")

		(OPTION_TS_SOURCEID, po::value<std::string>()->required(),
				"Source ID of the detector whose timestamp should be written into the final event and sent to the LKr for L1-triggers.")

		(OPTION_FIRST_BURST_ID, po::value<int>()->required(),
				"The current or first burst ID. This must be set if a PC starts during a run.")

		(OPTION_L1_REDUCTION_FACTOR, po::value<int>()->required(),
				"With this integer you can reduce the event rate going to L2 to a factor of 1/L1ReductionFactor. L1 Trigger will be processed every i event if  i++%reductionFactor==0")

		(OPTION_L2_REDUCTION_FACTOR, po::value<int>()->required(),
				"With this integer you can reduce the event rate accepted by L2 to a factor of 1/L1ReductionFactor. L2 Trigger will be processed every i event if  i++%reductionFactor==0")

		(OPTION_L1_DOWNSCALE_FACTOR, po::value<int>()->required(),
				"With this integer you can downscale the event rate accepted by L1 to a factor of 1/L1DownscaleFactor. L1 Trigger will accept every succeeded i event if  i++%downscaleFactor==0")

		(OPTION_L2_DOWNSCALE_FACTOR, po::value<int>()->required(),
				"With this integer you can downscale the event rate accepted by L2 to a factor of 1/L2DownscaleFactor. L2 Trigger will accept every succeeded i event if  i++%downscaleFactor==0")(
		OPTION_MIN_USEC_BETWEEN_L1_REQUESTS,
				po::value<int>()->default_value(1000),
				"Minimum time between two MRPs sent to the CREAMs")

		(OPTION_CREAM_MULTICAST_GROUP,
				po::value<std::string>()->default_value("239.1.1.1"),
				"Comma separated list of multicast group IPs for L1 requests to the CREAMs (MRP)")

		(OPTION_CREAM_MULTICAST_PORT, po::value<int>()->default_value(58914),
				"The port all L1 multicast MRPs to the CREAMs should be sent to")

		(OPTION_MAX_TRIGGERS_PER_L1MRP, po::value<int>()->default_value(100),
				"Maximum number of Triggers per L1 MRP")

		(OPTION_SEND_MRP_WITH_ZSUPPRESSION_FLAG,
				po::value<int>()->default_value(0),
				"Set to true if only zero-suppressed data from LKr should be requested after L1")

		(OPTION_MAX_NUMBER_OF_EVENTS_PER_BURST,
				po::value<int>()->default_value(2000000),
				"The number of events this pc should be able to receive. The system will ignore events with event numbers larger than this value")

		(OPTION_MERGER_HOST_NAMES, po::value<std::string>()->required(),
				"Comma separated list of IPs or hostnames of the merger PCs.")

		(OPTION_MERGER_PORT, po::value<int>()->required(),
				"The TCP port the merger is listening to.")

		(OPTION_ZMQ_IO_THREADS, po::value<int>()->default_value(1),
				"Number of ZMQ IO threads")

		(OPTION_PH_SCHEDULER, po::value<int>()->default_value(2),
				"Process scheduling policy to be used for the PacketHandler threads. 1: FIFO, 2: RR")

		(OPTION_ACTIVE_POLLING, po::value<int>()->default_value(1),
				"Use active polling (high CPU usage, might be faster depending on the number of pf_ring queues)")

		(OPTION_POLLING_DELAY, po::value<double>()->default_value(1E5),
				"Number of ticks to wait between two polls")

		(OPTION_POLLING_SLEEP_MICROS, po::value<int>()->default_value(1E4),
				"Number of microseconds to sleep if polling was unsuccessful during the last tries")

		(OPTION_MAX_FRAME_AGGREGATION, po::value<int>()->default_value(100000),
				"Maximum number of frames aggregated before spawning a TBB task to process them")

		(OPTION_MAX_AGGREGATION_TIME, po::value<int>()->default_value(100000),
				"Maximum time for one frame aggregation period before spawning a new TBB task in microseconds")

		(OPTION_INCREMENT_BURST_AT_EOB, po::value<bool>()->default_value(false),
				"Print out the source IDs and CREAM/crate IDs that have not been received during the last burst")

		(OPTION_L1_FLAG_MODE, po::value<bool>()->required(),
				"Enable flagging mode (No CUT) for L1 trigger.")

		(OPTION_STRAW_PORT, po::value<int>()->default_value(58916),

				"UDP-Port to be used to receive raw data stream coming from the Straws.")

		(OPTION_STRAW_ZMQ_PORT, po::value<int>()->default_value(58917),
				"ZMQ-Port to be used to forward raw data coming from the Straws to.")

		(OPTION_MUV_CREAM_CRATE_ID, po::value<int>()->default_value(-1),
				"Set the CREAM crate ID of which the data should be taken and put into the MUV1/Muv2 data blocks. Set to -1 to disable MUV1/Muv2 data acquisition.")

		(OPTION_STRAW_ZMQ_DST_HOSTS, po::value<std::string>()->required(),
				"Comma separated list of all hosts that have a ZMQ PULL socket listening to the strawZmqPort to receive STRAW data")

		(OPTION_PRINT_MISSING_SOURCES, po::value<bool>()->default_value(false),
				"If set to 1, information about unfinished events is written to /tmp/farm-logs/unfinishedEvents")

		(OPTION_WRITE_BROKEN_CREAM_INFO,
				po::value<bool>()->default_value(false),
				"If set to 1, information about non requested cream data (already received/not requested) is written to /tmp/farm-logs/nonRequestedCreamData)")

				;

		Options::Initialize(argc, argv, desc);
	}
};

} /* namespace na62 */

#endif /* MYOPTIONS_H_ */
