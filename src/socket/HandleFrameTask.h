/*
 * HandleFrameTask.h
 *
 *  Created on: Jun 27, 2014
 *      Author: Jonas Kunze (kunze.jonas@gmail.com)
 */

#ifndef HANDLEFRAMETASK_H_
#define HANDLEFRAMETASK_H_

#include <tbb/task.h>
#include <cstdint>
#include <atomic>

#include <socket/EthernetUtils.h>

namespace na62 {

class HandleFrameTask: public tbb::task {
private:
	std::vector<DataContainer> containers_;
	uint burstID_;

	void processARPRequest(struct ARP_HDR* arp);

	/**
	 * @return <true> If no checksum errors have been found
	 */
	bool checkFrame(struct UDP_HDR* hdr, uint16_t length);



	static uint16_t L0_Port;
	static uint16_t CREAM_Port;
	static uint16_t STRAW_PORT;
	static uint32_t MyIP;

	static std::atomic<uint> queuedTasksNum_;

	static uint highestSourceNum_;
	static std::atomic<uint64_t>* MEPsReceivedBySourceNum_;
	static std::atomic<uint64_t>* BytesReceivedBySourceNum_;

	void processFrame(DataContainer&& container);

public:
	HandleFrameTask(std::vector<DataContainer>&& _containers, uint burstID);
	virtual ~HandleFrameTask();

	tbb::task* execute();

	static void initialize();

	static inline uint getNumberOfQeuedTasks() {
		return queuedTasksNum_;
	}

	static inline uint64_t GetMEPsReceivedBySourceNum(uint8_t sourceNum) {
		return MEPsReceivedBySourceNum_[sourceNum];
	}

	static inline uint64_t GetBytesReceivedBySourceNum(uint8_t sourceNum) {
		return BytesReceivedBySourceNum_[sourceNum];
	}
};

} /* namespace na62 */

#endif /* HANDLEFRAMETASK_H_ */
