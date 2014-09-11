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

namespace na62 {
class Event;
struct EVENT_HDR;

namespace cream {
class LkrFragment;
} /* namespace cream */
} /* namespace na62 */

namespace tbb {
class spin_mutex;
} /* namespace tbb */

namespace zmq {
class socket_t;
} /* namespace zmq */

namespace na62 {

class StorageHandler {
public:
	static void initialize();
	static void onShutDown();

	static int SendEvent(const Event* event);

	static std::string GetMergerAddress();

private:
	static char* ResizeBuffer(char* buffer, const int oldLength,
			const int newLength);

	static std::vector<std::string> GetMergerAddresses();

	/**
	 * Generates the raw data as it should be send to the merger
	 */
	static EVENT_HDR* GenerateEventBuffer(const Event* event);

	static char* writeCreamData(char*& eventBuffer, uint& eventOffset,
			uint& eventBufferSize, uint& pointerTableOffset,
			cream::LkrFragment** fragments, uint numberOfFragments,
			uint sourceID);
	/*
	 * One Socket for every EventBuilder
	 */
	static std::vector<zmq::socket_t*> mergerSockets_;
	static tbb::spin_mutex sendMutex_;

	static std::atomic<uint> InitialEventBufferSize_;
	static int TotalNumberOfDetectors_;

};

} /* namespace na62 */

#endif /* STORAGEHANDLER_H_ */
