/*
 * TaskProcessor.h
 *
 *  Created on: Feb 9, 2016
 *      Author: giovanna
 */

#ifndef TASKPROCESSOR_H_
#define TASKPROCESSOR_H_

#include "HandleFrameTask.h"
#include "HandleFrameTaskL1.h"
#include <utils/AExecutable.h>
#include <tbb/concurrent_queue.h>

namespace na62 {

class TaskProcessor: public AExecutable {
public:
	TaskProcessor();
	virtual ~TaskProcessor();
	static tbb::concurrent_queue<HandleFrameTask*> TasksQueue_;
	static tbb::concurrent_queue<HandleFrameTaskL1*> TasksQueueL1_;

	static int getSize() {
		return TasksQueue_.unsafe_size() + TasksQueueL1_.unsafe_size();
	}
private:
	virtual void thread() override;
	virtual void onInterruption() override;
	std::atomic<bool> running_;
};

}

#endif
