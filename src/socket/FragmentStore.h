/*
 * FragmentStore.h
 *
 * This class handles IP fragmentation
 *
 *  Created on: Sep 29, 2014
 *      Author: Jonas Kunze (kunze.jonas@gmail.com)
 */

#ifndef FRAGMENTSTORE_H_
#define FRAGMENTSTORE_H_

#include <socket/EthernetUtils.h>
#include <structs/Network.h>
#include <sys/types.h>
#include <map>
#include <vector>
#include <tbb/spin_mutex.h>
#include <atomic>

#include <algorithm>
#include <iterator>

#include <options/Logging.h>

namespace na62 {

class FragmentStore {

public:
	static DataContainer addFragment(DataContainer&& fragment) {
		//LOG_ERROR("Fragmented packet from " << );
		UDP_HDR* hdr = (UDP_HDR*) fragment.data;
		const uint64_t fragID = generateFragmentID(hdr->ip.saddr, hdr->ip.id);
		const uint fragmentStoreNum = fragID % numberOfFragmentStores_;
		LOG_INFO("Fragmented packet from " << EthernetUtils::ipToString(hdr->ip.saddr));
		numberOfFragmentsReceived_++;

		/*
		 * Synchronize fragmentsById access
		 */
		tbb::spin_mutex::scoped_lock my_lock(
				newFragmentMutexes_[fragmentStoreNum]);

		auto& fragmentsReceived = fragmentsById_[fragmentStoreNum][fragID];

		fragmentsReceived.push_back(std::move(fragment));

		uint sumOfIPPayloadBytes = 0;
		UDP_HDR* lastFragment = nullptr;

		for (auto& frag : fragmentsReceived) {
			UDP_HDR* hdr = (UDP_HDR*) frag.data;

			sumOfIPPayloadBytes += ntohs(hdr->ip.tot_len) - sizeof(iphdr);

			if (!hdr->isMoreFragments()) {
				lastFragment = hdr;
			}
		}

		if (lastFragment != nullptr) {
			uint expectedPayloadSum = lastFragment->getFragmentOffsetInBytes()
					+ ntohs(lastFragment->ip.tot_len) - sizeof(iphdr);
			/*
			 * Check if we've received as many bytes as the offset of the last fragment plus its size
			 */
			if (expectedPayloadSum == sumOfIPPayloadBytes) {
				numberOfReassembledFrames_++;
				DataContainer reassembledFrame = reassembleFrame(
						fragmentsReceived);
				fragmentsById_[fragmentStoreNum].erase(
						generateFragmentID(hdr->ip.saddr, hdr->ip.id));
//				fragmentsReceived.clear();

				return reassembledFrame;
			}
		}

		return {nullptr, 0, false};
	}

	static uint getNumberOfReceivedFragments() {
		return numberOfFragmentsReceived_;
	}

	static uint getNumberOfReassembledFrames() {
		return numberOfReassembledFrames_;
	}

	static uint getNumberOfUnfinishedFrames() {
		uint sum = 0;
		for (auto store : fragmentsById_) {
			sum += store.size();
		}
		return sum;
	}

private:
	static const uint numberOfFragmentStores_ = 32;
	static std::map<uint64_t, std::vector<DataContainer>> fragmentsById_[numberOfFragmentStores_];
	static tbb::spin_mutex newFragmentMutexes_[numberOfFragmentStores_];

	static std::atomic<uint> numberOfFragmentsReceived_;
	static std::atomic<uint> numberOfReassembledFrames_;

	static inline uint64_t generateFragmentID(const uint_fast32_t srcIP,
			const uint_fast16_t fragID) {
		return (uint64_t) fragID | ((uint64_t) srcIP << 16);
	}

	static DataContainer reassembleFrame(std::vector<DataContainer> fragments) {
		/*
		 * Sort the fragments by offset
		 */
		std::sort(fragments.begin(), fragments.end(),
				[](const DataContainer& a, const DataContainer& b) {
					UDP_HDR* hdr1 = (UDP_HDR*) a.data;
					UDP_HDR* hdr2 = (UDP_HDR*) b.data;
					return hdr1->getFragmentOffsetInBytes() < hdr2->getFragmentOffsetInBytes();
				});

		UDP_HDR* lastFragment = (UDP_HDR*) fragments.back().data;

		/*
		 * We'll copy the ethernet and IP header of the first frame plus all IP-Payload of all frames
		 */
		uint_fast16_t totalBytes = sizeof(ether_header)
				+ lastFragment->getFragmentOffsetInBytes()
				+ ntohs(lastFragment->ip.tot_len);

		char* newFrameBuff = new char[totalBytes];

		uint_fast16_t currentOffset = sizeof(ether_header) + sizeof(iphdr);
		for (DataContainer& fragment : fragments) {
			UDP_HDR* currentData = (UDP_HDR*) fragment.data;

			if (currentData->getFragmentOffsetInBytes() + sizeof(ether_header)
					+ sizeof(iphdr) != currentOffset) {
				LOG_ERROR(
						   "Error while reassembling IP fragments: sum of fragment lengths is "
						<< currentOffset << " but offset of current frame is "
						<< currentData->getFragmentOffsetInBytes());

				for (DataContainer& fragment : fragments) {
					if (fragment.data != nullptr) {
						delete[] fragment.data;
					}
				}

				return {nullptr, 0, false};
			}

			/*
			 * Copy the payload of the IP datagram to the buffer
			 *
			 * The payload starts after sizeof(ether_header) + sizeof(iphdr) and is ntohs(currentData->ip.tot_len) - sizeof(iphdr) bytes long
			 */
			if (&fragment == &fragments.front()) {
				/*
				 * First frame is copied entirely
				 */
				memcpy(newFrameBuff, fragment.data, fragment.length);
				currentOffset = fragment.length;
			} else {
				memcpy(newFrameBuff + currentOffset,
						fragment.data + sizeof(ether_header) + sizeof(iphdr),
						ntohs(currentData->ip.tot_len) - sizeof(iphdr));
				currentOffset += ntohs(currentData->ip.tot_len) - sizeof(iphdr);
			}
			delete[] fragment.data;
		}
		return {newFrameBuff, currentOffset, true};
	}
};

} /* namespace na62 */

#endif /* FRAGMENTSTORE_H_ */
