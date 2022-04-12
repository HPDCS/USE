USE-IT
======

The Ultimate Share-Everything sImulaTion platform

## Brief
The Ultimate Share Everything Simulator is an x86-64 Open Source,
parallel simulation platform developed using C/POSIX
technology. The platform transparently supports all the mechanisms 
associated with parallelization and optimistic synchronization 
(e.g., state recoverability).

The programming model supported by USE-IT allows the simulation-model
developer to use a simple application-callback function named
`ProcessEvent()` as the event handler, whose parameters determine which
simulation object is currently taking control for processing its next
event, and where the state of this object is located in memory. In
USE-IT, a simulation object is a data structure, whose state can be
scattered on dynamically allocated memory chunks, hence the memory
address passed to the callback locates a top level data structure
implementing the object state-layout.

## Requirements

1. USE-IT is written in C standard.
2. USE-IT targets x86 platforms.
3. The build system is [Make](http://cmake.org), and we require version 2.8 or higher.

## Startup Instructions

1. Clone the repository to your local machine.
2. Make your model (see model/ReadMe.md for further information).
4. Adds rules about your model in Makefile, following the existing models.
3. Run your model as 'executable number_of_threads number_of_LPs'.

## Configuration

USE-IT is deployed with a basic configuration.
It is possible to modify the simulator configuration by passing the following macro to the makefile:


OPTIMISTIC_LEVEL:
defines the level of speculation used by the simulator, according the following values:
	0: it works in a conservative way.
	1: it is speculative on different LPs.
	2: it is fully speculative.

LOOKAHEAD:
defines the lookahead used by the simulation respect the the time unit.
note: if you want to use a lookahead, your model has to take it in account.

DEBUG:
if set to 1 enables debug features.

REPORT:
if set to 1 produce a full report at the end of the simulation.

LIST_NODE_PER_ALLOC:
defines the number of nodes (used by the sub-memory system) allocated in a single time.

CKP_PERIOD:
defines the check point interval, namely the number of events processed between two consecutive checkpoint.

CLEAN_CKP_INTERVAL:
defines the number of events processed before to perform a housekeeping clean of checkpoints.

PRUNE_PERIOD:
defines the number of events processed before to perform a housekeeping clean of the priority queue nodes.

ONGVT_PERIOD:
defines the number of events processed by an LP before to perform an OnGVT call.

PRINT_SCREEN:
if set to 1 enables colored prints.

NUMA: 
defining this macro the NUMA capabilities are enable: each LP is binded to a NUMA node is is managed by threads executing on this node.

PARALLEL_ALLOCATOR: 
defining this macro the a substet of the NUMA capabilities are enable: each LP is binded to a NUMA node, but any thread can execute it.

DISTRIBUTED_FETCH: 
if set to 1 defining this macro each thread is enabled to manage a partition of the whole LP set.

example:
make phold LOOKAHEAD=0.1 REPORT=1 DEBUG=0 CKP_PERIOD=10 PRINT_SCREEN=0




