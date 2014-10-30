/*
 * FragmentStore.cpp
 *
 *  Created on: Sep 29, 2014
 *      Author: root
 */

#include "FragmentStore.h"

namespace na62 {

const uint FragmentStore::numberOfFragmentStores_;
std::map<uint64_t, std::vector<DataContainer>> FragmentStore::fragmentsById_[];
tbb::spin_mutex FragmentStore::newFragmentMutexes_[];

std::atomic<uint> FragmentStore::numberOfFragmentsReceived_(0);
std::atomic<uint> FragmentStore::numberOfReassembledFrames_(0);

} /* namespace na62 */
