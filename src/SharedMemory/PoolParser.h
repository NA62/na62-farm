/*
 * TaskProcessor.cpp
 *
 *  Created on: May 16, 2016
 *      Author: marco
 */

#ifndef PoolParser_H_
#define PoolParser_H_

#include <boost/interprocess/ipc/message_queue.hpp>
#include <atomic>

#include "utils/AExecutable.h"

namespace na62 {

class PoolParser: public AExecutable {
public:
	PoolParser();
	virtual ~PoolParser();

private:
	virtual void thread() override;
	virtual void onInterruption() override;
	std::atomic<bool> running_;
	static uint highest_burst_id_received_;

};

}

#endif
