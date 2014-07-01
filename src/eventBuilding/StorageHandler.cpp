/*
 * StorageHandler.cpp
 *
 *  Created on: Mar 4, 2014
 \*      Author: Jonas Kunze (kunze.jonas@gmail.com)
 */

#include "StorageHandler.h"

#include <asm-generic/errno-base.h>
#include <bits/atomic_base.h>
#include <eventBuilding/Event.h>
#include <eventBuilding/SourceIDManager.h>
#include <glog/logging.h>
#include <l0/MEPEvent.h>
#include <l0/Subevent.h>
#include <LKr/LKREvent.h>
#include "../options/MyOptions.h"

#include <structs/Event.h>
#include <sys/types.h>
#include <zmq.h>
#include <cstdbool>
#include <cstring>
#include <iostream>
#include <string>

#include "../socket/ZMQHandler.h"
#include "EventBuilder.h"


namespace na62 {

std::vector<zmq::socket_t*> StorageHandler::MergerSockets_;

std::atomic<uint> StorageHandler::InitialEventBufferSize_;
int StorageHandler::TotalNumberOfDetectors_;

void freeZmqMessage(void *data, void *hint) {
	delete[]((char*)data);
}

void StorageHandler::Initialize() {
	LOG(INFO)<< "Connecting to merger: " << ZMQHandler::GetMergerAddress().c_str();
	for (int i = 0; i < Options::GetInt(OPTION_NUMBER_OF_EBS); i++) {
		zmq::socket_t* sock = ZMQHandler::GenerateSocket(ZMQ_PUSH);
		MergerSockets_.push_back(sock);
		sock->connect(ZMQHandler::GetMergerAddress().c_str());
	}

	/*
	 * L0 sources + LKr
	 */
	// TODO: Delete this as soon as the LKR is running
	if (SourceIDManager::NUMBER_OF_EXPECTED_CREAM_PACKETS_PER_EVENT == 0) {
		TotalNumberOfDetectors_ = SourceIDManager::NUMBER_OF_L0_DATA_SOURCES;
	} else {
		TotalNumberOfDetectors_ = SourceIDManager::NUMBER_OF_L0_DATA_SOURCES
		+ 1;
	}
	InitialEventBufferSize_ = 1000;
}

void StorageHandler::OnShutDown() {
	for (auto socket : MergerSockets_) {
		if (socket != nullptr) {
			socket->close();
			delete socket;
		}
	}
}

char* StorageHandler::ResizeBuffer(char* buffer, const int oldLength,
		const int newLength) {
	char* newBuffer = new char[newLength];
	memcpy(newBuffer, buffer, oldLength);
	delete[] buffer;
	return newBuffer;
}

int StorageHandler::SendEvent(const uint16_t& threadNum, Event* event) {
	/*
	 * TODO: Use multimessage instead of creating a separate buffer and copying the MEP data into it
	 */

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

	uint32_t sizeOfPointerTable = 4 * TotalNumberOfDetectors_;
	uint32_t pointerTableOffset = sizeof(struct EVENT_HDR);
	uint32_t eventOffset = sizeof(struct EVENT_HDR) + sizeOfPointerTable;

	for (int sourceNum = SourceIDManager::NUMBER_OF_L0_DATA_SOURCES - 1;
			sourceNum >= 0; sourceNum--) {
		l0::Subevent* subevent = event->getL0SubeventBySourceIDNum(sourceNum);

		if (eventOffset + 4 > eventBufferSize) {
			eventBuffer = ResizeBuffer(eventBuffer, eventBufferSize,
					eventBufferSize + 1000);
			eventBufferSize += 1000;
		}

		/*
		 * Put the sub-detector  into the pointer table
		 */
		uint32_t eventOffset32 = eventOffset / 4;
		std::memcpy(eventBuffer + pointerTableOffset, &eventOffset32, 3);
		std::memset(eventBuffer + pointerTableOffset + 3,
				SourceIDManager::SourceNumToID(sourceNum), 1);
		pointerTableOffset += 4;

		/*
		 * Write the L0 data
		 */
		int payloadLength;
		for (int i = subevent->getNumberOfParts() - 1; i >= 0; i--) {
			l0::MEPEvent* e = subevent->getPart(i);
			payloadLength = e->getEventLength()
					- sizeof(struct l0::MEPEVENT_RAW_HDR)
					+ sizeof(struct L0_BLOCK_HDR);
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
					e->getData(), payloadLength - sizeof(struct L0_BLOCK_HDR));
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
	if (eventOffset + 4 > eventBufferSize) {
		eventBuffer = ResizeBuffer(eventBuffer, eventBufferSize,
				eventBufferSize + 1000);
		eventBufferSize += 1000;
	}

	if (SourceIDManager::NUMBER_OF_EXPECTED_CREAM_PACKETS_PER_EVENT > 0) {
		uint32_t eventOffset32 = eventOffset / 4;
		/*
		 * Put the LKr into the pointer table
		 */
		std::memcpy(eventBuffer + pointerTableOffset, &eventOffset32, 3);
		std::memset(eventBuffer + pointerTableOffset + 3, SOURCE_ID_LKr, 1); // 0x24 is the LKr sourceID

		for (int localCreamID = event->getNumberOfZSuppressedLKrEvents() - 1;
				localCreamID >= 0; localCreamID--) {
			cream::LKREvent* e = event->getZSuppressedLKrEvent(localCreamID);

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

	header->length = eventLength / 4;

	zmq::message_t zmqMessage((void*) eventBuffer, eventLength,
			(zmq::free_fn*) freeZmqMessage);

	while (true) {
		try {
			MergerSockets_[threadNum]->send(zmqMessage);
			break;
		} catch (const zmq::error_t& ex) {
			if (ex.num() != EINTR) { // try again if EINTR (signal caught)
				LOG(ERROR)<< ex.what();

				for (uint i = 0; i !=EventBuilder::NUMBER_OF_EBS; i++) {
					MergerSockets_[i]->close();
					delete MergerSockets_[i];
				}
				return 0;
			}
		}
	}

	return header->length * 4;
}
} /* namespace na62 */
