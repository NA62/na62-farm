/*
 * StorageHandler.h
 *
 *  Created on: Mar 4, 2014
 \*      Author: Jonas Kunze (kunze.jonas@gmail.com)
 */

#ifndef STORAGEHANDLER_H_
#define STORAGEHANDLER_H_

#include <zmq.hpp>
#include <atomic>
#include <cstdint>
#include <vector>

namespace na62 {
class Event;
} /* namespace na62 */

namespace na62 {

class StorageHandler {
public:
	static void Initialize();
	static void OnShutDown();

	static int SendEvent(const uint16_t& thredNum, Event* event);

private:
	static char* ResizeBuffer(char* buffer, const int oldLength, const int newLength);
	/*
	 * One Socket for every EventBuilder
	 */
	static std::vector<zmq::socket_t*> MergerSockets_;

	static std::atomic<uint> InitialEventBufferSize_;
	static int TotalNumberOfDetectors_;
};

} /* namespace na62 */

#endif /* STORAGEHANDLER_H_ */
