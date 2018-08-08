/*
 * PcapDump.cpp
 *
 *  Created on: 4 Dec 2017
 *      Author: root
 */

#include "PcapDump.h"
#include <ctime>

namespace na62 {

void PcapDump::dumpPacket(char* packet, uint packet_leght) {
  pcap_pkthdr pcap_hdr;
  pcap_hdr.caplen = pcap_hdr.len = packet_leght;
  gettimeofday(&pcap_hdr.ts, NULL);
  pcap_dump((u_char *)dumper_, &pcap_hdr, (u_char *) packet);
}

PcapDump::PcapDump(std::string pathbasefilename, short id):filename_(pathbasefilename + "-" + std::to_string(std::time(0)) + "-"  + std::to_string(id) + ".pcap") {
	handle_ = pcap_open_dead(DLT_EN10MB, 1 << 16);
	dumper_ = pcap_dump_open(handle_, filename_.c_str());
}

PcapDump::~PcapDump() {
	  pcap_dump_close(dumper_);
}

} /* namespace na62 */
