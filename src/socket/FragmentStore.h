/*
 * FragmentStore.h
 *
 *  Created on: Sep 29, 2014
 *      Author: root
 */

#ifndef FRAGMENTSTORE_H_
#define FRAGMENTSTORE_H_

#include <socket/EthernetUtils.h>
#include <structs/Network.h>
#include <sys/types.h>
#include <map>
#include <vector>
#include <tbb/spin_mutex.h>

#include <algorithm>
#include <iterator>

namespace na62 {

class FragmentStore {

public:
	static DataContainer addFragment(DataContainer&& fragment) {
		tbb::spin_mutex::scoped_lock my_lock(newFragmentMutex_);
		numberOfFragmentsReceived_++;

		UDP_HDR* hdr = (UDP_HDR*) fragment.data;
		auto& fragmentVector = fragmentsById_[hdr->ip.id];

		fragmentVector.push_back(std::move(fragment));

		uint sumOfPayloadBytes = 0;
		UDP_HDR* lastFragment = nullptr;

		for (auto& frag : fragmentsById_[hdr->ip.id]) {
			UDP_HDR* hdr = (UDP_HDR*) frag.data;

			sumOfPayloadBytes += ntohs(hdr->ip.tot_len) - sizeof(iphdr);

			if (!hdr->isMoreFragments()) {
				lastFragment = hdr;
			}
		}
		if (lastFragment != nullptr) {
			uint expectedPayloadSum = lastFragment->getFragmentOffsetInBytes()
					+ ntohs(lastFragment->ip.tot_len) - sizeof(iphdr);
			if (expectedPayloadSum == sumOfPayloadBytes) {
				numberOfReassembledFrames_++;
				DataContainer reassembledFrame = reassembleFrame(
						fragmentVector);
				fragmentsById_.erase(hdr->ip.id);
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

private:
	static std::map<ushort, std::vector<DataContainer>> fragmentsById_;
	static tbb::spin_mutex newFragmentMutex_;

	static uint numberOfFragmentsReceived_;
	static uint numberOfReassembledFrames_;
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
		uint16_t totalBytes = sizeof(ether_header)
				+ lastFragment->getFragmentOffsetInBytes()
				+ ntohs(lastFragment->ip.tot_len);

		char* newFrameBuff = new char[totalBytes];

		uint16_t currentOffset = sizeof(ether_header) + sizeof(iphdr);
		for (DataContainer& fragment : fragments) {
			UDP_HDR* currentData = (UDP_HDR*) fragment.data;

			if (currentData->getFragmentOffsetInBytes() + sizeof(ether_header)
					+ sizeof(iphdr) != currentOffset) {
				std::cerr
						<< "Error while reassembling IP fragments: sum of fragment lengths is "
						<< currentOffset << " but offset of current frame is "
						<< currentData->getFragmentOffsetInBytes() << std::endl;

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
