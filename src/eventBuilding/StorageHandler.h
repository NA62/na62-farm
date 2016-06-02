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
#include <mutex>
#include <utils/AExecutable.h>
#include <tbb/concurrent_queue.h>
#include <storage/EventSerializer.h>

namespace zmq {
class socket_t;
class message_t;
} /* namespace zmq */

namespace na62 {
class Event;

class StorageHandler: public AExecutable {

public:
	StorageHandler() {running_ = true;}
	static void initialize();
	static void onShutDown();

	static int SendEvent(const Event* event);

	/**
	 * Change the list of mergers to be used for sending data to
	 * @param mergerList comma or semicolon separated list of hostnames or IPs of the mergers to be used
	 */
	static void setMergers(std::string mergerList);

private:
	virtual void thread() override;
	virtual void onInterruption() override;
	std::atomic<bool> running_;

	static std::vector<std::string> GetMergerAddresses(std::string mergerList);
	static tbb::concurrent_queue<const EVENT_HDR*> DataQueue_;
	/*
	 * One Socket for every EventBuilder
	 */
	static std::vector<zmq::socket_t*> mergerSockets_;
	static std::recursive_mutex sendMutex_;
};

} /* namespace na62 */

#endif /* STORAGEHANDLER_H_ */
