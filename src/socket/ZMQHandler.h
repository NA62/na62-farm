/*
 * ZMQHandler.h
 *
 *  Created on: Feb 21, 2014
 *      Author: Jonas Kunze (kunze.jonas@gmail.com)
 */

#ifndef ZMQHANDLER_H_
#define ZMQHANDLER_H_

#include <boost/thread/pthread/mutex.hpp>
#include <set>
#include <string>

namespace zmq {
class context_t;
class socket_t;
} /* namespace zmq */

namespace na62 {

class ZMQHandler {
public:
	static void Initialize(const int numberOfIOThreads);

	static void Destroy();
	static zmq::socket_t* GenerateSocket(int socketType, int highWaterMark=100000);

	static std::string GetEBL0Address(int threadNum);
	static std::string GetEBLKrAddress(int threadNum);
	static std::string GetMergerAddress();

	/*
	 * Binds the socket to the specified address and stores the enables connections to this address
	 */
	static void BindInproc(zmq::socket_t* socket, std::string address);

	/*
	 * Connects to the specified address as soon as the address has been bound
	 */
	static void ConnectInproc(zmq::socket_t* socket, std::string address);
private:
	static zmq::context_t* context_;
	static std::set<std::string> boundAddresses_;
	static boost::mutex connectMutex_;
};

} /* namespace na62 */

#endif /* ZMQHANDLER_H_ */
