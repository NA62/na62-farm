/*
 * StorageHandler.cpp
 *
 *  Created on: Mar 4, 2014
 \*      Author: Jonas Kunze (kunze.jonas@gmail.com)
 */

#include "StorageHandler.h"

#include <boost/algorithm/string.hpp>
#include <asm-generic/errno-base.h>
#include <eventBuilding/Event.h>
#include <eventBuilding/SourceIDManager.h>
#include <tbb/spin_mutex.h>
#include <sstream>

#include <l0/MEPFragment.h>
#include <l0/Subevent.h>
#include <LKr/LkrFragment.h>
#include <structs/Event.h>
#include <zmq.h>
#include <zmq.hpp>
#include <cstdbool>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <socket/ZMQHandler.h>
#include <glog/logging.h>

#include "../options/MyOptions.h"

namespace na62 {

std::vector<zmq::socket_t*> StorageHandler::mergerSockets_;

std::atomic<uint> StorageHandler::InitialEventBufferSize_;
int StorageHandler::TotalNumberOfDetectors_;

tbb::spin_mutex StorageHandler::sendMutex_;

std::vector<std::string> StorageHandler::GetMergerAddresses(
		std::string mergerList) {
	std::vector<std::string> mergers;
	boost::split(mergers, mergerList, boost::is_any_of(";,"));

	if (mergers.empty()) {
		LOG_ERROR << "List of running mergers is empty => Stopping now!"
				<< ENDL;
		;
		exit(1);
	}

	std::vector<std::string> addresses;
	for (std::string host : mergers) {
		std::stringstream address;
		address << "tcp://" << host << ":"
				<< Options::GetInt(OPTION_MERGER_PORT);
		addresses.push_back(address.str());
	}
	return addresses;
}

void StorageHandler::setMergers(std::string mergerList) {
	tbb::spin_mutex::scoped_lock my_lock(sendMutex_);
	for (auto socket : mergerSockets_) {
		ZMQHandler::DestroySocket(socket);
	}
	mergerSockets_.clear();

	for (std::string address : GetMergerAddresses(mergerList)) {
		LOG_INFO<< "Connecting to merger: " << address;
		zmq::socket_t* socket = ZMQHandler::GenerateSocket("StorageHandler", ZMQ_PUSH);
		socket->connect(address.c_str());
		mergerSockets_.push_back(socket);
	}

}

void StorageHandler::initialize() {
	setMergers(Options::GetString(OPTION_MERGER_HOST_NAMES));

	/*
	 * L0 sources + LKr
	 */
	if (SourceIDManager::NUMBER_OF_EXPECTED_CREAM_PACKETS_PER_EVENT == 0) {
		TotalNumberOfDetectors_ = SourceIDManager::NUMBER_OF_L0_DATA_SOURCES;
	} else {
		TotalNumberOfDetectors_ = SourceIDManager::NUMBER_OF_L0_DATA_SOURCES
				+ 1;
	}

	if (SourceIDManager::MUV1_NUMBER_OF_FRAGMENTS != 0) {
		TotalNumberOfDetectors_++;
	}

	if (SourceIDManager::MUV2_NUMBER_OF_FRAGMENTS != 0) {
		TotalNumberOfDetectors_++;
	}

	InitialEventBufferSize_ = 1000;
}

void StorageHandler::onShutDown() {
	for (auto socket : mergerSockets_) {
		ZMQHandler::DestroySocket(socket);
	}
	mergerSockets_.clear();
}

char* StorageHandler::ResizeBuffer(char* buffer, const int oldLength,
		const int newLength) {
	char* newBuffer = new char[newLength];
	memcpy(newBuffer, buffer, oldLength);
	delete[] buffer;
	return newBuffer;
}

EVENT_HDR* StorageHandler::GenerateEventBuffer(const Event* event) {

	uint eventBufferSize = InitialEventBufferSize_;
	char* eventBuffer = new char[InitialEventBufferSize_];

	struct EVENT_HDR* header = (struct EVENT_HDR*) eventBuffer;

	header->eventNum = event->getEventNumber();
	header->format = 0x62; // TODO: update current format
	// header->length will be written later on
	header->burstID = event->getBurstID();
	header->timestamp = event->getTimestamp();
	header->triggerWord = event->getTriggerTypeWord();
	header->reserved1 = 0;
	header->fineTime = event->getFinetime();
	header->numberOfDetectors = TotalNumberOfDetectors_;
	header->reserved2 = 0;
	header->processingID = event->getProcessingID();
	header->SOBtimestamp = 0; // Will be set by the merger

	uint sizeOfPointerTable = 4 * TotalNumberOfDetectors_;
	uint pointerTableOffset = sizeof(struct EVENT_HDR);
	uint eventOffset = sizeof(struct EVENT_HDR) + sizeOfPointerTable;

	for (int sourceNum = 0;
			sourceNum != SourceIDManager::NUMBER_OF_L0_DATA_SOURCES;
			sourceNum++) {
		l0::Subevent* subevent = event->getL0SubeventBySourceIDNum(sourceNum);

		if (eventOffset + 4 > eventBufferSize) {
			eventBuffer = ResizeBuffer(eventBuffer, eventBufferSize,
					eventBufferSize + 1000);
			eventBufferSize += 1000;
		}

		/*
		 * Put the sub-detector  into the pointer table
		 */
		uint eventOffset32 = eventOffset / 4;
		std::memcpy(eventBuffer + pointerTableOffset, &eventOffset32, 3);
		std::memset(eventBuffer + pointerTableOffset + 3,
				SourceIDManager::sourceNumToID(sourceNum), 1);
		pointerTableOffset += 4;

		/*
		 * Write the L0 data
		 */
		int payloadLength;
		for (uint i = 0; i != subevent->getNumberOfFragments(); i++) {
			l0::MEPFragment* e = subevent->getFragment(i);
			payloadLength = e->getPayloadLength() + sizeof(struct L0_BLOCK_HDR);
			if (eventOffset + payloadLength > eventBufferSize) {
				eventBuffer = ResizeBuffer(eventBuffer, eventBufferSize,
						eventBufferSize + payloadLength);
				eventBufferSize += payloadLength;
			}

			struct L0_BLOCK_HDR* blockHdr = (struct L0_BLOCK_HDR*) (eventBuffer
					+ eventOffset);
			blockHdr->dataBlockSize = payloadLength;
			blockHdr->sourceSubID = e->getSourceSubID();
			blockHdr->reserved = 0;

			memcpy(eventBuffer + eventOffset + sizeof(struct L0_BLOCK_HDR),
					e->getPayload(),
					payloadLength - sizeof(struct L0_BLOCK_HDR));
			eventOffset += payloadLength;

			/*
			 * 32-bit alignment
			 */
			if (eventOffset % 4 != 0) {
				memset(eventBuffer + eventOffset, 0, eventOffset % 4);
				eventOffset += eventOffset % 4;
			}
		}
	}

	/*
	 * Write the LKr data
	 */
	if (SourceIDManager::NUMBER_OF_EXPECTED_LKR_CREAM_FRAGMENTS != 0) {
		writeCreamData(eventBuffer, eventOffset, eventBufferSize,
				pointerTableOffset, event->getZSuppressedLkrFragments(),
				event->getNumberOfZSuppressedLkrFragments(), SOURCE_ID_LKr);
	}

	if (SourceIDManager::MUV1_NUMBER_OF_FRAGMENTS != 0) {
		writeCreamData(eventBuffer, eventOffset, eventBufferSize,
				pointerTableOffset, event->getMuv1Fragments(),
				event->getNumberOfMuv1Fragments(), SOURCE_ID_MUV1);
	}

	if (SourceIDManager::MUV2_NUMBER_OF_FRAGMENTS != 0) {
		writeCreamData(eventBuffer, eventOffset, eventBufferSize,
				pointerTableOffset, event->getMuv2Fragments(),
				event->getNumberOfMuv2Fragments(), SOURCE_ID_MUV2);
	}

	/*
	 * Trailer
	 */
	EVENT_TRAILER* trailer = (EVENT_TRAILER*) (eventBuffer + eventOffset);
	trailer->eventNum = event->getEventNumber();
	trailer->reserved = 0;

	const int eventLength = eventOffset + 4/*trailer*/;

	if (eventBufferSize > InitialEventBufferSize_) {
		InitialEventBufferSize_ = eventBufferSize;
	}

	/*
	 * header may have been overwritten -> redefine it
	 */
	header = (struct EVENT_HDR*) eventBuffer;

	header->length = eventLength / 4;

	return header;
}

char* StorageHandler::writeCreamData(char*& eventBuffer, uint& eventOffset,
		uint& eventBufferSize, uint& pointerTableOffset,
		cream::LkrFragment** fragments, uint numberOfFragments, uint sourceID) {
	/*
	 * Write the LKr data
	 */
	if (eventOffset + 4 > eventBufferSize) {
		eventBuffer = ResizeBuffer(eventBuffer, eventBufferSize,
				eventBufferSize + 1000);
		eventBufferSize += 1000;
	}

	uint eventOffset32 = eventOffset / 4;
	/*
	 * Put the LKr into the pointer table
	 */
	std::memcpy(eventBuffer + pointerTableOffset, &eventOffset32, 3);
	std::memset(eventBuffer + pointerTableOffset + 3, sourceID, 1);
	pointerTableOffset += 4;

	for (uint fragmentNum = 0; fragmentNum != numberOfFragments;
			fragmentNum++) {
		cream::LkrFragment* e = fragments[fragmentNum];

		if (eventOffset + e->getEventLength() > eventBufferSize) {
			eventBuffer = ResizeBuffer(eventBuffer, eventBufferSize,
					eventBufferSize + e->getEventLength());
			eventBufferSize += e->getEventLength();
		}

		memcpy(eventBuffer + eventOffset, e->getDataWithHeader(),
				e->getEventLength());
		eventOffset += e->getEventLength();

		/*
		 * 32-bit alignment
		 */
		if (eventOffset % 4 != 0) {
			memset(eventBuffer + eventOffset, 0, eventOffset % 4);
			eventOffset += eventOffset % 4;
		}
	}

	return eventBuffer;
}

int StorageHandler::SendEvent(const Event* event) {
	/*
	 * TODO: Use multimessage instead of creating a separate buffer and copying the MEP data into it
	 */
	const EVENT_HDR* data = GenerateEventBuffer(event);

	/*
	 * Send the event to the merger with a zero copy message
	 */
	zmq::message_t zmqMessage((void*) data, data->length * 4,
			(zmq::free_fn*) ZMQHandler::freeZmqMessage);

	while (ZMQHandler::IsRunning()) {
		tbb::spin_mutex::scoped_lock my_lock(sendMutex_);
		try {
			mergerSockets_[event->getBurstID() % mergerSockets_.size()]->send(
					zmqMessage);
			break;
		} catch (const zmq::error_t& ex) {
			if (ex.num() != EINTR) { // try again if EINTR (signal caught)
				LOG_ERROR << ex.what() << ENDL;

				onShutDown();
				return 0;
			}
		}
	}

	return data->length * 4;
}
} /* namespace na62 */
