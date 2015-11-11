/*
 * FarmStatistics.cpp
 *
 *  Created on: 05.11.2015
 *      Author: Tassilo
 */

#include "FarmStatistics.h"

#include <tbb/task.h>
#include <tbb/tick_count.h>
#include <tbb/tbb_thread.h>
#include <sys/types.h>
#include <algorithm>
#include <iostream>
#include <thread>
#include <string>
#include <stdio.h>
#include <time.h>

#include <boost/timer/timer.hpp>

std::atomic<uint> FarmStatistics::PH;
std::atomic<uint> FarmStatistics::T;
std::atomic<uint> FarmStatistics::LB;
std::atomic<uint> FarmStatistics::LP;
std::vector<statisticTimeStamp> FarmStatistics::recvTimes;
std::vector<statisticTimeStamp> FarmStatistics::recvTimesBuff;

FarmStatistics::FarmStatistics() :
	running_(true) {
}

FarmStatistics::~FarmStatistics() {
	// TODO Auto-generated destructor stub
}

FarmStatistics::init() {
	FarmStatistics::timer.start();
	FarmStatistics::hostname = getHostName();
	FarmStatistics::recvTimes.reserve(200);
	FarmStatistics::recvTimesBuff.reserve(200);
}

FarmStatistics::thread() {
	ofstream myfile;
	myfile.open(currentDateTime + hostname + ".txt");
	while (running_) {
		//TODO add CAS
		if (FarmStatistics::recvTimes.size >= 200) {
			FarmStatistics::recvTimesBuff = FarmStatistics::recvTimes;
			FarmStatistics::recvTimes.clear();
			for(statisticTimeStamp a : recvTimesBuff){
				myfile << getFileOutString(a);
			}
		}

	}
	myfile.close();
}

static void addTime(std::string comment) {
	st.time = FarmStatistics::timer.elapsed().wall;
	st.comment = comment;
	FarmStatistics::recvTimes.push_back(st);
}

static uint getID(timeSource source) {
	uint idNo;
	switch (source) {
	case PacketHandler:
		idNo = FarmStatistics::PH.fetch_add(1, std::memory_order_relaxed);
		break;
	case Task:
		idNo = FarmStatistics::T.fetch_add(1, std::memory_order_relaxed);
		break;
	case L0Build:
		idNo = FarmStatistics::LB.fetch_add(1, std::memory_order_relaxed);
		break;
	case L0Process:
		idNo = FarmStatistics::LP.fetch_add(1, std::memory_order_relaxed);
		break;
	}
	return idNo;
}

static char* getHostName() {
	//20 chars fit all hostnames
	char hostname[20];
	result = gethostname(hostname, 20);
	if (result) {
		perror("gethostname");
		return nullptr;
	}
	return hostname;
}

// Buid the line to be written into the logfile
static std::string getFileOutString(statisticTimeStamp sts){
	std::string fos(sts.comment + ", \ttime: " + sts.time + "\n");
	return fos;
}

// Get current date/time, format is YYYY-MM-DD.HH:mm:ss
static std::string currentDateTime() {
    time_t     now = time(0);
    struct tm  tstruct;
    char       buf[80];
    tstruct = *localtime(&now);
    strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);

    return buf;
}
