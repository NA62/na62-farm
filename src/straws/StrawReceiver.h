/*
 * StrawReceiver.h
 *
 *  Created on: May 27, 2014
 *      Author: Jonas Kunze (kunze.jonas@gmail.com)
 */

#ifndef STRAWRECEIVER_H_
#define STRAWRECEIVER_H_

#include <tbb/spin_mutex.h>
 #include <string>

namespace zmq {
class socket_t;
} /* namespace zmq */

namespace na62 {
struct DataContainer;
} /* namespace na62 */

namespace na62 {

class StrawReceiver {
public:
	StrawReceiver();
	virtual ~StrawReceiver();

	static void processFrame(DataContainer&& data);
	static void initialize();
	static void onShutDown();

private:
	static zmq::socket_t* mergerSocket_;
	static tbb::spin_mutex sendMutex_;

	std::string getZmqAddress();
};

} /* namespace na62 */

#endif /* STRAWRECEIVER_H_ */
