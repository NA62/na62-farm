/*
 * StorageHandler.h
 *
 *  Created on: Mar 4, 2014
 \*      Author: Jonas Kunze (kunze.jonas@gmail.com)
 */

#ifndef STORAGEHANDLER_H_
#define STORAGEHANDLER_H_

#include <sys/types.h>
#include <atomic>
#include <thread>
#include <tbb/spin_mutex.h>
#include <tbb/mutex.h>

namespace zmq {
class socket_t;
} /* namespace zmq */

namespace na62 {
class Event;
} /* namespace na62 */

namespace na62 {

class StorageHandler {
public:
	static void Initialize();
	static void OnShutDown();

	static int SendEvent(Event* event);

private:
	static char* ResizeBuffer(char* buffer, const int oldLength,
			const int newLength);
	/*
	 * One Socket for every EventBuilder
	 */
	static zmq::socket_t* MergerSocket_;
	static tbb::spin_mutex sendMutex_;

	static std::atomic<uint> InitialEventBufferSize_;
	static int TotalNumberOfDetectors_;
};

} /* namespace na62 */

#endif /* STORAGEHANDLER_H_ */
