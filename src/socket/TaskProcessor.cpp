/*
 * TaskProcessor.cpp
 *
 *  Created on: Feb 9, 2016
 *      Author: giovanna
 */

#include "TaskProcessor.h"
#include "HandleFrameTask.h"
#include <boost/timer/timer.hpp>

namespace na62 {
tbb::concurrent_queue<HandleFrameTask*> TaskProcessor::TasksQueue_;

TaskProcessor::TaskProcessor(uint task_processor_id):running_(true),task_processor_id_(task_processor_id), dumper_("/var/log/dumped-packets/packets", task_processor_id) {}

TaskProcessor::~TaskProcessor(){}

void TaskProcessor::thread() {
		while (running_) {
			HandleFrameTask* task;
			if (TaskProcessor::TasksQueue_.try_pop(task)) {
				task->execute(this);
				delete task;
			} else {
				boost::this_thread::sleep(boost::posix_time::microsec(50));
			}
		}
	}

void TaskProcessor::dumpPacket(DataContainer container) {
	dumper_.dumpPacket(container.data, container.length);
}

void TaskProcessor::onInterruption() {
		running_ = false;
}

}
