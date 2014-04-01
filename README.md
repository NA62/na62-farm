NA62 PC-Farm
========

## Introduction
The NA62 PC-Farm framework is the interface between the raw data produced by the CERN experiment NA62, 
the online software trigger algorithms running on the PC farm at the experiment and the data storage located
at the experiment (~100 TB buffer) and at the main data center at CERN.


## Dataflow
After a positive L0 trigger processed within the readout hardware, all subdetectors send the raw data of 
the accepted event within Multi Event Packets (MEPs) to the PC-farm. As the subdetectors have several
readout boards (typically TEL62 boards) the raw data coming from one subdetector is transported within several
MEPs. As the number of readout boards is fixed for every detector, the PC farm will wait until it received data
from every readout board (each board is defined by a unique sourceID). This process is called event building.

As soon as the event is complete, the L1 trigger algorithms will be executed with the built event.

As the LKr detector only sends a reduced data set after the L0 trigger, the PC farm requests the full data set 
in case of a positive L1 trigger. Thereby a multicas UDP datagram is sent to the LKr readout boards (CREAMs) and 
those boards response with the requested event data.
As soon as all CREAMs have sent the requested data to the PC which has sent the request, this PC will execute the L2 
trigger algorithms with the now complete event. If the event passes the L2 algorithms it will be passed to the 
merger PC. This PC collects all events produced within one burst (the time NA62 receives a continous proton beam is of 
the order of 10 s after which a break of roughly 20 s follows). The created burst files will be stored on a local 
disk buffer and than sent to the central data center of CERN. The maximum output data rate of the merger PC is limited 
by the network (10 Gb/s) and by the writing spead of the tape heads at the data center (about 200 MB/s).

## Design of the framework
The framework consists of following components:
  * na62-farm-lib (static library)
  * na62-trigger-algorithms (static library)
  * na62-farm
  * na62-merger
  * na62-farm-dim-interface

### na62-farm-lib
This static library is used by all other software components. It stores all data types for the communications protocols
and usefull helper classes.

### na62-trigger-algorithms
Here all L1 and L2 trigger algorithms are implemented. These are executed with the incoming raw data received by the na62-farm.

### na62-farm
This is the main program running on the PC-farm machines. It receives the MEPs, processes the trigger algorithms and 
sends the built events to the merger.

### na62-merger
This is the main program running on the merger PC. It receives the accepted events from the PC-farm, generates files 
with all events of one bursts and stores those on the local disk buffer. The sending to the CERN data center is done
by cronjobs.

### na62-farm-dim-interface
The NA62 collaboration decided to use DIM to distribute status updates of all components within the NA62 experiment.
The design of the na62-framework is designed to use the high performance malloc implementation by google: tc_malloc. 
Unfortunately the DIM framework is not compatible with tc_malloc and therefore has to be run in a seperate process.
The na62-farm-dim-interface communicates with na62-farm and na62-merger using IPC. This way those components can send
and receive status updates.
