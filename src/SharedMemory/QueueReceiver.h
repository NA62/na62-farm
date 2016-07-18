/*
 * TaskProcessor.cpp
 *
 *  Created on: May 16, 2016
 *      Author: marco
 */

#ifndef QUEUERECEIVER_H_
#define QUEUERECEIVER_H_

#include <boost/interprocess/ipc/message_queue.hpp>
#include <atomic>

#include "utils/AExecutable.h"

namespace na62 {

class QueueReceiver: public AExecutable {
public:
	QueueReceiver();
	virtual ~QueueReceiver();


private:
	virtual void thread() override;
	virtual void onInterruption() override;
	std::atomic<bool> running_;
};

}

#endif
