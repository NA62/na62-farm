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
#define OPTION_L0_RECEIVER_PORT (char*)"L0Port"
#define OPTION_CREAM_RECEIVER_PORT (char*)"CREAMPort"
#define OPTION_EOB_BROADCAST_IP (char*)"EOBBroadcastIP"
#define OPTION_EOB_BROADCAST_PORT (char*)"EOBBroadcastPort"

/*
 * Event Building
 */
#define OPTION_NUMBER_OF_EBS (char*)"numberOfEB"
#define OPTION_DATA_SOURCE_IDS (char*)"L0DataSourceIDs"

#define OPTION_TS_SOURCEID (char*)"timestampSourceID"

#define OPTION_CREAM_CRATES (char*)"CREAMCrates"

#define OPTION_FIRST_BURST_ID (char*)"firstBurstID"

#define OPTION_CREAM_MULTICAST_GROUP (char*)"creamMulticastIP"
#define OPTION_CREAM_MULTICAST_PORT (char*)"creamMulticastPort"
#define OPTION_MAX_TRIGGERS_PER_L1MRP (char*)"maxTriggerPerL1MRP"

#define OPTION_NUMBER_OF_EVENTS_PER_BURST_EXPECTED (char*)"numberOfEventsPerBurstExpected"

/*
 * Triggering
 */
#define OPTION_L1_DOWNSCALE_FACTOR  (char*)"L1DownscaleFactor"
#define OPTION_L2_DOWNSCALE_FACTOR  (char*)"L2DownscaleFactor"

#define OPTION_MIN_USEC_BETWEEN_L1_REQUESTS (char*)"minUsecsBetweenL1Requests"

/*
 * Merger
 */
#define OPTION_MERGER_HOST_NAME (char*)"mergerHostName"
#define OPTION_MERGER_PORT (char*)"mergerPort"

/*
 * Performance
 */
#define OPTION_ZMQ_IO_THREADS (char*)"zmqIoThreads"

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

		(OPTION_L0_RECEIVER_PORT, po::value<int>()->default_value(58913),
				"UDP-Port for L1 data reception")

		(OPTION_EOB_BROADCAST_IP, po::value<std::string>()->required(),
				"Broadcast IP for the distribution of the last event number. Should be the broadcast IP of a network attached to NICs with standard kernel drivers (e.g. farm-out).")

		(OPTION_EOB_BROADCAST_PORT, po::value<int>()->default_value(14162),
				"Port for Broadcast packets used to distribute the last event numbers ob a burst.")

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

		(OPTION_TS_SOURCEID, po::value<std::string>()->required(),
				"Source ID of the detector which timestamp should be written into the final event and sent to the LKr for L1-triggers.")

		(OPTION_FIRST_BURST_ID, po::value<int>()->required(),
				"The current or first burst ID. This must be set if a PC starts during a run.")

		(OPTION_L1_DOWNSCALE_FACTOR, po::value<int>()->required(),
				"With this integer you can downscale the event rate going to L2 to a factor of 1/L1DownscaleFactor. The L1 Trigger will accept every even if  i++%downscaleFactor==0")

		(OPTION_L2_DOWNSCALE_FACTOR, po::value<int>()->required(),
				"With this integer you can downscale the event rate accepted by L2 to a factor of 1/L1DownscaleFactor. The L2 Trigger will accept every even if  i++%downscaleFactor==0")

		(OPTION_MIN_USEC_BETWEEN_L1_REQUESTS,
				po::value<int>()->default_value(1000),
				"Minimum time between two MRPs sent to the CREAMs")

		(OPTION_CREAM_MULTICAST_GROUP, po::value<std::string>()->required(),
				"The multicast group IP for L1 requests to the CREAMs (MRP)")

		(OPTION_CREAM_MULTICAST_PORT, po::value<int>()->default_value(58914),
				"The port all L1 multicast MRPs to the CREAMs should be sent to")

		(OPTION_MAX_TRIGGERS_PER_L1MRP, po::value<int>()->default_value(100),
				"Maximum number of Triggers per L1 MRP")

		(OPTION_NUMBER_OF_EVENTS_PER_BURST_EXPECTED,
				po::value<int>()->default_value(10000000),
				"Expected number of events per burst and PC")

		(OPTION_MERGER_HOST_NAME, po::value<std::string>()->required(),
				"IP or hostname of the merger PC.")

		(OPTION_MERGER_PORT, po::value<int>()->required(),
				"The TCP port the merger is listening to.")

		(OPTION_ZMQ_IO_THREADS, po::value<int>()->default_value(1),
				"Number of ZMQ IO threads")

				;

		Options::Initialize(argc, argv, desc);
	}
};

} /* namespace na62 */

#endif /* MYOPTIONS_H_ */
