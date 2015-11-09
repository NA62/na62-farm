/*
 * FarmStatistics.h
 *
 *  Created on: 05.11.2015
 *      Author: Tassilo
 */

#ifndef FARMSTATISTICS_H_
#define FARMSTATISTICS_H_

struct statisticTimeStamp {
	std::string comment;
	u_int32_t time;
};

class FarmStatistics {
public:
	FarmStatistics();
	virtual ~FarmStatistics();
	void init();
	static std::string getID(timeSource);
	static void addTime(std::string);

	void stopRunning() {
		running_ = false;
	}

	static std::atomic<uint> PH;
	static std::atomic<uint> T;
	static std::atomic<uint> LB;
	static std::atomic<uint> LP;

	enum timeSource:int { PacketHandler, Task, L0Build, L0Process };
	static boost::timer::cpu_timer timer;
	bool running_;
	const char hostname;

private:
	static std::vector<statisticTimeStamp> recvTimes;
	static std::vector<statisticTimeStamp> recvTimesBuff;
	static char* getHostName();
	static std::string getFileOutString(statisticTimeStamp sts)
	const std::string currentDateTime()
};

#endif /* FARMSTATISTICS_H_ */
