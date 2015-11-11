/*
 * FarmStatistics.h
 *
 *  Created on: 05.11.2015
 *      Author: Tassilo
 */

#ifndef FARMSTATISTICS_H_
#define FARMSTATISTICS_H_

#include <cstdlib>
#include <sys/types.h>
#include <atomic>
#include <string>
#include <utils/AExecutable.h>
#include <boost/timer/timer.hpp>

struct statisticTimeStamp {
	std::string comment;
	u_int32_t time;
};

namespace na62 {
class FarmStatistics: public AExecutable {
public:
	FarmStatistics();
	virtual ~FarmStatistics();
	static void init();
	enum timeSource
		:int {PacketHandler, Task, L0Build, L0Process
	};
	static uint getID(int i);
	static void addTime(std::string);



	static std::atomic<uint> PH;
	static std::atomic<uint> T;
	static std::atomic<uint> LB;
	static std::atomic<uint> LP;

	static boost::timer::cpu_timer timer;
	static bool running_;
	static const char* hostname;

	static std::vector<statisticTimeStamp> recvTimes;

	static void startRunning() {
		running_ = true;
	}
	static void stopRunning() {
		running_ = false;
	}
	void thread();
private:
	static std::vector<statisticTimeStamp> recvTimesBuff;
	static char* getHostName();
	static std::string getFileOutString(statisticTimeStamp sts);
	static std::string currentDateTime();
};
}

#endif /* FARMSTATISTICS_H_ */
