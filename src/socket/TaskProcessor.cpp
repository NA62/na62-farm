/*
 * TaskProcessor.cpp
 *
 *  Created on: Feb 9, 2016
 *      Author: giovanna
 */

#include "TaskProcessor.h"
#include <boost/timer/timer.hpp>

namespace na62 {
tbb::concurrent_queue<HandleFrameTask*> TaskProcessor::TasksQueue_;
tbb::concurrent_queue<HandleFrameTaskL1*> TaskProcessor::TasksQueueL1_;


TaskProcessor::TaskProcessor(){
	running_ = true;
}

TaskProcessor::~TaskProcessor(){}

void TaskProcessor::thread() {
		while (running_) {
			HandleFrameTask* task;
			HandleFrameTaskL1* taskl1;
			if (TaskProcessor::TasksQueue_.try_pop(task)) {
				task->execute();
				delete task;
			}
			if (TaskProcessor::TasksQueueL1_.try_pop(taskl1)) {
				taskl1->execute();
				delete taskl1;
			}
			else {
				boost::this_thread::sleep(boost::posix_time::microsec(50));
			}
		}
	}

void TaskProcessor::onInterruption() {
		running_ = false;
		}

}
