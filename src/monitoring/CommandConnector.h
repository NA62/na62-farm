/*
 * CommandConnector.h
 *
 *  Created on: Jul 25, 2012
 *      Author: Jonas Kunze (kunze.jonas@gmail.com)
 */

#pragma once
#ifndef COMMANDCONNECTOR_H_
#define COMMANDCONNECTOR_H_

#include <boost/noncopyable.hpp>
#include <utils/AExecutable.h>

namespace na62 {

class CommandConnector: private boost::noncopyable, public AExecutable  {
public:
	CommandConnector();
	virtual ~CommandConnector();
	void run();
private:
	void thread();
};

} /* namespace na62 */
#endif /* COMMANDCONNECTOR_H_ */
