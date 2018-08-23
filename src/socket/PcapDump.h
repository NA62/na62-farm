/*
 * PcapDump.h
 *
 *  Created on: 4 Dec 2017
 *      Author: root
 */

#ifndef SRC_SOCKET_PCAPDUMP_H_
#define SRC_SOCKET_PCAPDUMP_H_

#include <pcap/pcap.h>
#include <string>


namespace na62 {

class PcapDump {
public:
	PcapDump(std::string pathbasefilename, short id);
	virtual ~PcapDump();

	void dumpPacket(char* packet, uint packet_leght);

private:
	std::string filename_;
	pcap_t* handle_;
	pcap_dumper_t* dumper_;

};

} /* namespace na62 */

#endif /* SRC_SOCKET_PCAPDUMP_H_ */
