/*
 * HandleFrameTask.h
 *
 *  Created on: Jun 27, 2014
 *      Author: root
 */

#ifndef HANDLEFRAMETASK_H_
#define HANDLEFRAMETASK_H_

#include <tbb/task.h>

#include <socket/EthernetUtils.h>

namespace na62 {

class HandleFrameTask: public tbb::task {
private:
	DataContainer container;

	void processARPRequest(struct ARP_HDR* arp);

	/**
	 * @return <true> If no checksum errors have been found
	 */
	bool checkFrame(struct UDP_HDR* hdr, uint16_t length);

	static uint16_t L0_Port;
	static uint16_t CREAM_Port;
	static uint16_t EOB_BROADCAST_PORT;

public:
	HandleFrameTask(DataContainer&&  _container);
	virtual ~HandleFrameTask();

	tbb::task* execute();

	static void Initialize();
};

} /* namespace na62 */

#endif /* HANDLEFRAMETASK_H_ */
