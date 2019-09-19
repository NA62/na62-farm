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
#define OPTION_GATEWAY_MAC_ADDRESS (char *)"gatewayMAC"

#define OPTION_L0_RECEIVER_PORT (char*)"L0Port"
#define OPTION_CREAM_RECEIVER_PORT (char*)"CREAMPort"

/*
 * Event Building
 */
//#define OPTION_NUMBER_OF_EBS (char*)"numberOfEB"
#define OPTION_DATA_SOURCE_IDS (char*)"L0DataSourceIDs"
#define OPTION_L1_DATA_SOURCE_IDS (char*)"L1DataSourceIDs"

#define OPTION_TS_SOURCEID (char*)"timestampSourceID"

#define OPTION_CREAM_CRATES (char*)"CREAMCrates"
//#define OPTION_INACTIVE_CREAM_CRATES (char*)"inactiveCREAMCrates"

#define OPTION_FIRST_BURST_ID (char*)"firstBurstID"
#define OPTION_CURRENT_RUN_NUMBER (char*)"currentRunNumber"

#define OPTION_CREAM_MULTICAST_GROUP (char*)"creamMulticastIP"
#define OPTION_CREAM_MULTICAST_PORT (char*)"creamMulticastPort"
#define OPTION_MAX_TRIGGERS_PER_L1MRP (char*)"maxTriggerPerL1MRP"

#define OPTION_SEND_MRP_WITH_ZSUPPRESSION_FLAG (char*)"sendMRPsWithZSuppressionFlag"

#define OPTION_INCREMENT_BURST_AT_EOB (char*)"incrementBurstAtEOB"

#define OPTION_MIN_USEC_BETWEEN_L1_REQUESTS (char*)"minUsecsBetweenL1Requests"
#define OPTION_UNICAST_ADDRESS (char*)"unicastIP"

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
 * EOB
 */
#define OPTION_FLUSH_BURST_MILLIS (char*)"flushBurstMillis"
#define OPTION_CLEAN_BURST_MILLIS (char*)"cleanBurstMillis"

/*
 * MUVs
*/
#define OPTION_MUV_CREAM_CRATE_ID (char*)"muvCreamCrateID"

/*
 *  STRAW

#define OPTION_STRAW_PORT (char*)"strawReceivePort"
#define OPTION_STRAW_ZMQ_PORT (char*)"strawZmqPort"
#define OPTION_STRAW_ZMQ_DST_HOSTS (char*)"strawZmqDstHosts"
 */
/*
 * Debugging
 */
#define OPTION_PRINT_MISSING_SOURCES (char*)"printMissingSources"
#define OPTION_DUMP_BAD_PACKETS (char*)"dumpBadPackets"

//#define OPTION_WRITE_BROKEN_CREAM_INFO (char*)"printBrokenCreamInfo"

//For eventPool building

#define OPTION_MAX_NUMBER_OF_EVENTS_PER_BURST (char*)"maxNumberOfEventsPerBurst"
#define OPTION_NUMBER_OF_FRAGS_PER_L0MEP (char*)"numberOfFragmentsPerMEP"
#define OPTION_FARM_HOST_NAMES (char*)"farmHostNames"

namespace na62 {
class MyOptions: public Options {
public:
	MyOptions();
	virtual ~MyOptions();

	static void Load(int argc, char* argv[]) {

		desc.add_options()

		(OPTION_CONFIG_FILE,
				po::value<std::string>()->default_value("/etc/na62-farm.conf"),
				"Config file for the options shown here")

		(OPTION_ETH_DEVICE_NAME,
				po::value<std::string>()->default_value("zc:enp130s0f0"),
				"Name of the device to be used for receiving data")

		(OPTION_GATEWAY_MAC_ADDRESS,
				po::value<std::string>()->default_value("00:11:22:33:44:55"),
				"MAC address of gateway")

		(OPTION_L0_RECEIVER_PORT, po::value<int>()->default_value(58913),
				"UDP-Port for L1 data reception")

		(OPTION_FARM_HOST_NAMES, po::value<std::string>()->required(),
				"Comma separated list of IPs or hostnames of the farm PCs.")

		(OPTION_MAX_NUMBER_OF_EVENTS_PER_BURST, po::value<int>()->default_value(2000000),
				"The number of events this pc should be able to receive. The system will ignore events with event numbers larger than this value")

		(OPTION_NUMBER_OF_FRAGS_PER_L0MEP, po::value<int>()->default_value(8),
				"The number of fragments excepted in each L0 MEP fragment")

//		(OPTION_NUMBER_OF_EBS, po::value<int>()->default_value(boost::thread::hardware_concurrency() - 4),
//				"Number of threads to be used for event building and L1/L2 processing")

		(OPTION_DATA_SOURCE_IDS, po::value<std::string>()->required(),
				"Comma separated list of all available L0 data source IDs sending Data to L1 together with the expected numbers of packets per source. The format is like following (A,B,C are sourceIDs and a,b,c are the number of expected packets per source):\n \t A:a,B:b,C:c")

		(OPTION_L1_DATA_SOURCE_IDS, po::value<std::string>()->default_value(""),
						"Comma separated list of all available data source IDs sending Data to L1 together with the expected numbers of packets per source. The format is like following (A,B,C are sourceIDs and a,b,c are the number of expected packets per source):\n \t A:a,B:b,C:c")

		(OPTION_CREAM_RECEIVER_PORT, po::value<int>()->default_value(58915),
				"UDP-Port for L2 CREAM data reception")

		(OPTION_CREAM_CRATES, po::value<std::string>()->default_value("0:0"),
				"Defines the expected sourceIDs within the data packets from the CREAMs. The format is $crateID1:$CREAMIDs,$crateID1:$CREAMIDs,$crateID2:$CREAMIDs... E.g. 1:2-4,1:11-13,2:2-5,2:7 for two crates (1 and 2) with following IDs (2,3,4,11,12,13 and 2,3,4,5,7).")

//		(OPTION_INACTIVE_CREAM_CRATES,
//				po::value<std::string>()->default_value(""),
//				"Defines a list of CREAMs that must appear in the normal creamCrate list but should not be activated")

		(OPTION_TS_SOURCEID, po::value<std::string>()->default_value("0x40"),
				"Source ID of the detector whose timestamp should be written into the final event and sent to the LKr for L1-triggers.")

		(OPTION_FIRST_BURST_ID, po::value<int>()->required(),
				"The current or first burst ID. This must be set if a PC starts during a run.")

		(OPTION_CURRENT_RUN_NUMBER, po::value<int>()->default_value(1),
				"The current RUN.")

		(OPTION_MIN_USEC_BETWEEN_L1_REQUESTS,
				po::value<int>()->default_value(1000),
				"Minimum time between two MRPs sent to the L1")
		(OPTION_UNICAST_ADDRESS,
				po::value<std::string>()->required(),
				"Comma separated list of IPs to send the L1 requests(MRP)")

		(OPTION_MERGER_HOST_NAMES, po::value<std::string>()->required(),
				"Comma separated list of IPs or host names of the merger PCs.")

		(OPTION_MERGER_PORT, po::value<int>()->required(),
				"The TCP port the merger is listening to.")

		(OPTION_CREAM_MULTICAST_GROUP,
				po::value<std::string>()->required(),
				"Comma separated list of multicast group IPs for L1 requests to the L1 (MRP)")

//		(OPTION_CREAM_UNICAST_LIST,
//				po::value<std::string>()->required(),
//				"Comma separated list of IPs for L1 requests to the L1 (MRP)")

		(OPTION_CREAM_MULTICAST_PORT, po::value<int>()->default_value(58914),
				"The port all L1 multicast MRPs to the CREAMs should be sent to")

		(OPTION_MAX_TRIGGERS_PER_L1MRP, po::value<int>()->default_value(100),
				"Maximum number of Triggers per L1 MRP")

		(OPTION_SEND_MRP_WITH_ZSUPPRESSION_FLAG,
				po::value<int>()->default_value(0),
				"Set to true if only zero-suppressed data from LKr should be requested after L1")

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
				"Maximum number of frames aggregated before spawning a task to process them")

		(OPTION_MAX_AGGREGATION_TIME, po::value<int>()->default_value(100000),
				"Maximum time for one frame aggregation period before spawning a new task in microseconds")

		(OPTION_INCREMENT_BURST_AT_EOB, po::value<bool>()->default_value(false),
				"Print out the source IDs and CREAM/crate IDs that have not been received during the last burst")

//		(OPTION_STRAW_PORT, po::value<int>()->default_value(58916),
//				"UDP-Port to be used to receive raw data stream coming from the Straws.")

//		(OPTION_STRAW_ZMQ_PORT, po::value<int>()->default_value(58917),
//				"ZMQ-Port to be used to forward raw data coming from the Straws to.")

//		(OPTION_MUV_CREAM_CRATE_ID, po::value<int>()->default_value(-1),
//				"Set the CREAM crate ID of which the data should be taken and put into the MUV1/Muv2 data blocks. Set to -1 to disable MUV1/Muv2 data acquisition.")

//		(OPTION_STRAW_ZMQ_DST_HOSTS, po::value<std::string>()->required(),
//				"Comma separated list of all hosts that have a ZMQ PULL socket listening to the strawZmqPort to receive STRAW data")

		(OPTION_PRINT_MISSING_SOURCES, po::value<bool>()->default_value(false),
				"If set to 1, information about unfinished events is written to /tmp/farm-logs/unfinishedEvents")
		(OPTION_DUMP_BAD_PACKETS, po::value<bool>()->default_value(false),
				"If set to 1, information bad packet are dumped to /var/log/dumped-packets")
		(OPTION_FLUSH_BURST_MILLIS, po::value<int>()->default_value(3000),
				"Number of microseconds after the EOB to start flushing data")
		(OPTION_CLEAN_BURST_MILLIS, po::value<int>()->default_value(5000),
				"Number of microseconds after the EOB to cleanup the event pool")
//		(OPTION_WRITE_BROKEN_CREAM_INFO,
//				po::value<bool>()->default_value(false),
//				"If set to 1, information about non requested cream data (already received/not requested) is written to /tmp/farm-logs/nonRequestedCreamData)")

				;

		Options::Initialize(argc, argv, desc);
	}
};

} /* namespace na62 */

#endif /* MYOPTIONS_H_ */
