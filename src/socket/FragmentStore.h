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
	static DataContainer&& addFragment(DataContainer&& fragment) {
		UDP_HDR* hdr = (UDP_HDR*) fragment.data;
		const uint64_t fragID = generateFragmentID(hdr->ip.saddr, hdr->ip.id);
		const uint fragmentStoreNum = fragID % numberOfFragmentStores_; // In this way you will alwais have a number from 0 to numberOfFragmentStores_
		numberOfFragmentsReceived_++;
		LOG_INFO("Fragmented packet from " << EthernetUtils::ipToString(hdr->ip.saddr) << " fragmentStoreNum " << fragmentStoreNum << " fragID " << fragID);

		/*
		 * Synchronize fragmentsById access
		 */
		tbb::spin_mutex::scoped_lock my_lock(
				newFragmentMutexes_[fragmentStoreNum]);

		auto& fragmentsReceived = fragmentsById_[fragmentStoreNum][fragID];
		fragmentsReceived.push_back(std::move(fragment));

		uint sumOfIPPayloadBytes = 0;
		UDP_HDR* lastFragment = nullptr;
		//LOG_INFO("Fragmented packet from " << EthernetUtils::ipToString(hdr->ip.saddr) << " Number of fragments received " << fragmentsReceived.size());
		uint_fast8_t last_fragment_counter = 0;
		for (auto& frag : fragmentsReceived) {
			UDP_HDR* tmp_hdr = (UDP_HDR*) frag.data;
			sumOfIPPayloadBytes += ntohs(tmp_hdr->ip.tot_len) - sizeof(iphdr);

			if (!tmp_hdr->isMoreFragments()) {
				lastFragment = tmp_hdr;
				++last_fragment_counter;
				//LOG_INFO("Fragmented packet from " << EthernetUtils::ipToString(tmp_hdr->ip.saddr) << " is the last fragment");
			}
		}

		if (last_fragment_counter > 1) {
			//We have received too many final fragments purging the memory
			LOG_ERROR("Fragmented packet from " << EthernetUtils::ipToString(hdr->ip.saddr) << " Too many last fragments.. " << last_fragment_counter);
			cleanAll(fragmentStoreNum, fragID);
			return std::move(empty());
		}

		if (lastFragment != nullptr) {
			//LOG_INFO("Fragmented packet from " << EthernetUtils::ipToString(hdr->ip.saddr) << " the last packet is arrived, number of fragments received " << fragmentsReceived.size());
			uint expectedPayloadSum = lastFragment->getFragmentOffsetInBytes()
					+ ntohs(lastFragment->ip.tot_len) - sizeof(iphdr);
			/*
			 * Check if we've received as many bytes as the offset of the last fragment plus its size
			 */
			//LOG_INFO("Fragmented packet from " << EthernetUtils::ipToString(hdr->ip.saddr) << " expectedPayloadSum: " << expectedPayloadSum << " sumOfIPPayloadBytes " << sumOfIPPayloadBytes );
			if (expectedPayloadSum == sumOfIPPayloadBytes) {
				numberOfReassembledFrames_++;
				DataContainer reassembledFrame = reassembleFrame(fragmentStoreNum, fragID);
				cleanAll(fragmentStoreNum, fragID);
				return std::move(reassembledFrame);
			}
		}
		return std::move(empty());
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

	static DataContainer&& empty(){
		DataContainer empty{nullptr, 0, false};
		return std::move(empty);
	}

	static void cleanAll(const uint fragmentStoreNum, const uint64_t fragID) {
		//Free the fragments memory
		for (auto& frag : fragmentsById_[fragmentStoreNum][fragID]) {
			if (frag.data != nullptr) {
				frag.free();
			}
		}
		fragmentsById_[fragmentStoreNum][fragID].clear();
		fragmentsById_[fragmentStoreNum].erase(fragID);

//		if (fragmentsById_[fragmentStoreNum].find(fragID) == fragmentsById_[fragmentStoreNum].end()) {
//			LOG_ERROR("Fragmented packet from " << EthernetUtils::ipToString(hdr->ip.saddr) << " Element " << fragID << " correctly deleted ");
//		} else {
//			LOG_ERROR("Fragmented packet from " << EthernetUtils::ipToString(hdr->ip.saddr) << " !!!!! Element " << fragID << " not deleted ");
//		}
		LOG_INFO("Cleaned fragmentStoreNum: " << fragmentStoreNum << " fragID: " << fragID);
	}

	static DataContainer&& reassembleFrame(const uint fragmentStoreNum, const uint64_t fragID) {
		auto& fragments = fragmentsById_[fragmentStoreNum][fragID];
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
				return std::move(empty());
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
		}
		LOG_INFO("Returning the assembled packet");
		DataContainer complete_fragment{newFrameBuff, currentOffset, true};
		return std::move(complete_fragment);
	}
};

} /* namespace na62 */

#endif /* FRAGMENTSTORE_H_ */
