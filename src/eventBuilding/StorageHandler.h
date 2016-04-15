/*
 * StorageHandler.h
 *
 *  Created on: Mar 4, 2014
 \*      Author: Jonas Kunze (kunze.jonas@gmail.com)
 */

#ifndef STORAGEHANDLER_H_
#define STORAGEHANDLER_H_

#include <sys/types.h>
#include <zmq.hpp>
#include <atomic>
#include <string>
#include <vector>

namespace tbb {
class spin_mutex;
} /* namespace tbb */

namespace zmq {
class socket_t;
} /* namespace zmq */

namespace na62 {
class Event;

class StorageHandler {
public:
	static void initialize();
	static void onShutDown();

	static int SendEvent(const Event* event);

	static std::string GetMergerAddress();

	/**
	 * Change the list of mergers to be used for sending data to
	 * @param mergerList comma or semicolon separated list of hostnames or IPs of the mergers to be used
	 */
	static void setMergers(std::string mergerList);

private:
	static std::vector<std::string> GetMergerAddresses(std::string mergerList);

	/*
	 * One Socket for every EventBuilder
	 */
	static std::vector<zmq::socket_t*> mergerSockets_;
	static tbb::spin_mutex sendMutex_;
};

} /* namespace na62 */

#endif /* STORAGEHANDLER_H_ */
