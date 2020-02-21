#Versione test - non uso ottimizzazioni del compilatore

CC=gcc
#FLAGS=-g -Wall -pthread -lm

FLAGS= -DARCH_X86_64 -g3 -Wall -Wextra -mrtm -mno-red-zone -O0
#-DCACHE_LINE_SIZE="getconf LEVEL1_DCACHE_LINESIZE"

#CLS = 64#"getconf LEVEL1_DCACHE_LINESIZE"
FLAGS:=$(FLAGS) -DCACHE_LINE_SIZE=$(shell getconf LEVEL1_DCACHE_LINESIZE) -DN_CPU=$(shell grep -c ^processor /proc/cpuinfo)

INCLUDE=-Iinclude/ -Imm/ -Icore/ -Istatistics/ -Ireverse/ -Idatatypes
LIBS=-pthread -lm -lcap
ARCH_X86=1
ARCH_X86_64=1


ifdef MAX_SKIPPED_LP
CFLAGS:=$(FLAGS) -DMAX_SKIPPED_LP=$(MAX_SKIPPED_LP)
else
CFLAGS:=$(FLAGS) -DMAX_SKIPPED_LP=100000
endif

ifndef ORIGINAL
PREEMPT_COUNTER=1
LONG_JMP=1
HANDLE_INTERRUPT=1
HANDLE_INTERRUPT_WITH_CHECK=1
endif

STATISTICS_ADDED=1

ifdef STATISTICS_ADDED
CFLAGS:= $(CFLAGS) -DSTATISTICS_ADDED=$(STATISTICS_ADDED)
else
CFLAGS:= $(CFLAGS) -DSTATISTICS_ADDED=0
endif

#invalidation in O(1)
ifdef CONSTANT_CHILD_INVALIDATION
CFLAGS:= $(CFLAGS) -DCONSTANT_CHILD_INVALIDATION=$(CONSTANT_CHILD_INVALIDATION)
NEW_CONSTANT_CHILD_INVALIDATION=$(CONSTANT_CHILD_INVALIDATION)
else
CFLAGS:= $(CFLAGS) -DCONSTANT_CHILD_INVALIDATION=0
NEW_CONSTANT_CHILD_INVALIDATION=0
endif

#POSTING
ifdef POSTING
CFLAGS:= $(CFLAGS) -DPOSTING=$(POSTING)
NEW_POSTING=$(POSTING)
else
CFLAGS:= $(CFLAGS) -DPOSTING=0
NEW_POSTING=0
endif

ifdef PREEMPT_COUNTER
CFLAGS:= $(CFLAGS) -DPREEMPT_COUNTER=$(PREEMPT_COUNTER)
NEW_PREEMPT_COUNTER=$(PREEMPT_COUNTER)
else
CFLAGS:= $(CFLAGS) -DPREEMPT_COUNTER=0
endif

ifeq ($(NEW_PREEMPT_COUNTER),1)

ifdef LONG_JMP
CFLAGS:= $(CFLAGS) -DLONG_JMP=$(LONG_JMP)
NEW_LONG_JMP=$(LONG_JMP)
else
CFLAGS:= $(CFLAGS) -DLONG_JMP=0
NEW_LONG_JMP=0
endif

else
#IPI_PREEMPT_COUNTER==0
CFLAGS:= $(CFLAGS) -DLONG_JMP=0
NEW_LONG_JMP=0
endif


ifeq ($(NEW_LONG_JMP),1)
ifdef HANDLE_INTERRUPT
CFLAGS:= $(CFLAGS) -DHANDLE_INTERRUPT=$(HANDLE_INTERRUPT)
NEW_HANDLE_INTERRUPT=$(HANDLE_INTERRUPT)
else
CFLAGS:= $(CFLAGS) -DHANDLE_INTERRUPT=0
NEW_HANDLE_INTERRUPT=0
endif

else
CFLAGS:= $(CFLAGS) -DHANDLE_INTERRUPT=0
NEW_HANDLE_INTERRUPT=0
endif


ifeq ($(NEW_HANDLE_INTERRUPT),1)

ifeq ($(NEW_POSTING),1)

ifdef HANDLE_INTERRUPT_WITH_CHECK
CFLAGS:= $(CFLAGS) -DHANDLE_INTERRUPT_WITH_CHECK=$(HANDLE_INTERRUPT_WITH_CHECK)
NEW_HANDLE_INTERRUPT_WITH_CHECK=$(HANDLE_INTERRUPT_WITH_CHECK)
else
CFLAGS:= $(CFLAGS) -DHANDLE_INTERRUPT_WITH_CHECK=0
NEW_HANDLE_INTERRUPT_WITH_CHECK=0
endif

else 
#NEW_POSTING=0
CFLAGS:= $(CFLAGS) -DHANDLE_INTERRUPT_WITH_CHECK=0
NEW_HANDLE_INTERRUPT_WITH_CHECK=0
endif

else
#NEW_HANDLE_INTERRUPT=0
CFLAGS:= $(CFLAGS) -DHANDLE_INTERRUPT_WITH_CHECK=0
NEW_HANDLE_INTERRUPT_WITH_CHECK=0
endif

ifeq ($(NEW_HANDLE_INTERRUPT_WITH_CHECK),1)

ifdef IPI_SUPPORT
CFLAGS:= $(CFLAGS) -DIPI_SUPPORT=$(IPI_SUPPORT)
NEW_IPI_SUPPORT=$(IPI_SUPPORT)
else
CFLAGS:= $(CFLAGS) -DIPI_SUPPORT=0
NEW_IPI_SUPPORT=0
endif

ifdef SYNCH_CHECK
CFLAGS:= $(CFLAGS) -DSYNCH_CHECK=$(SYNCH_CHECK)
NEW_SYNCH_CHECK=$(SYNCH_CHECK)
else
CFLAGS:= $(CFLAGS) -DSYNCH_CHECK=0
NEW_SYNCH_CHECK=0
endif

else
#NEW_HANDLE_INTERRUPT_WITH_CHECK=0
CFLAGS:= $(CFLAGS) -DIPI_SUPPORT=0
NEW_IPI_SUPPORT=0

CFLAGS:= $(CFLAGS) -DSYNCH_CHECK=0
NEW_SYNCH_CHECK=0
endif

ifeq ($(NEW_IPI_SUPPORT),1)
ifdef DECISION_MODEL
CFLAGS:= $(CFLAGS) -DDECISION_MODEL=$(DECISION_MODEL)
NEW_DECISION_MODEL=$(DECISION_MODEL)
else
CFLAGS:= $(CFLAGS) -DDECISION_MODEL=0
NEW_DECISION_MODEL=0
endif

else
CFLAGS:= $(CFLAGS) -DDECISION_MODEL=0
NEW_DECISION_MODEL=0
endif

ifdef OPTIMISTIC_LEVEL
CFLAGS:=$(CFLAGS)  -DOPTIMISTIC_MODE=$(OPTIMISTIC_LEVEL)
else
CFLAGS:=$(CFLAGS)  -DOPTIMISTIC_MODE=1
endif

ifdef MALLOC
CFLAGS:=$(CFLAGS) -DNO_DYMELOR -DMALLOC=1
else
CFLAGS:=$(CFLAGS)  -DMALLOC=0
endif

ifdef REVERSIBLE
CFLAGS:= $(CFLAGS) -DREVERSIBLE=$(REVERSIBLE)
else
CFLAGS:= $(CFLAGS) -DREVERSIBLE=0
endif

ifdef VERBOSE
CFLAGS:=$(CFLAGS) -DVERBOSE=$(VERBOSE)
else
CFLAGS:=$(CFLAGS) -DVERBOSE=0
endif

ifdef LOOKAHEAD
CFLAGS:= $(CFLAGS) -DLOOKAHEAD=$(LOOKAHEAD)
endif

ifdef DEBUG

ifeq ($(DEBUG),0)
CFLAGS:= $(CFLAGS) -DDEBUG=0 -DNDEBUG
else
CFLAGS:= $(CFLAGS) -DDEBUG=$(DEBUG)
endif

else
CFLAGS:= $(CFLAGS) -DDEBUG=0 -DNDEBUG
endif

ifdef REPORT
CFLAGS:= $(CFLAGS) -DREPORT=$(REPORT)
else
CFLAGS:= $(CFLAGS) -DREPORT=1
endif

ifdef LIST_NODE_PER_ALLOC
CFLAGS:= $(CFLAGS) -DLIST_NODE_PER_ALLOC=$(LIST_NODE_PER_ALLOC)
endif

ifdef CHECKPOINT_PERIOD
CFLAGS:= $(CFLAGS) -DCHECKPOINT_PERIOD=$(CHECKPOINT_PERIOD)
else
CFLAGS:= $(CFLAGS) -DCHECKPOINT_PERIOD=50
endif

ifdef CLEAN_CKP_INTERVAL
CFLAGS:= $(CFLAGS) -DCLEAN_CKP_INTERVAL=$(CLEAN_CKP_INTERVAL)
endif

ifdef PRUNE_PERIOD
CFLAGS:= $(CFLAGS) -DPRUNE_PERIOD=$(PRUNE_PERIOD)
else
CFLAGS:= $(CFLAGS) -DPRUNE_PERIOD=50
endif

ifdef ONGVT_PERIOD
CFLAGS:= $(CFLAGS) -DONGVT_PERIOD=$(ONGVT_PERIOD)
else
CFLAGS:= $(CFLAGS) -DONGVT_PERIOD=-1
endif


ifdef PRINT_SCREEN
CFLAGS:= $(CFLAGS) -DPRINT_SCREEN=$(PRINT_SCREEN)
else
CFLAGS:= $(CFLAGS) -DPRINT_SCREEN=1
endif

################################# NB CALQUEUE ##########################

#NB_CALQUEUE
ifdef PERC_USED_BUCKET
CFLAGS:= $(CFLAGS) -DPUB=$(PERC_USED_BUCKET)
else
CFLAGS:= $(CFLAGS) -DPUB=0.33
endif

#NB_CALQUEUE
ifdef ELEM_PER_BUCKET
CFLAGS:= $(CFLAGS) -DEPB=$(ELEM_PER_BUCKET)
else
CFLAGS:= $(CFLAGS) -DEPB=3
endif

#NB_CALQUEUE
ifdef ENABLE_PRUNE
CFLAGS:= $(CFLAGS) -DENABLE_PRUNE=$(ENABLE_PRUNE)
else
CFLAGS:= $(CFLAGS) -DENABLE_PRUNE=1
endif


#NB_CALQUEUE
ifdef ENABLE_PRUNE
CFLAGS:= $(CFLAGS) -DENABLE_EXPANSION=$(ENABLE_EXPANSION)
else
CFLAGS:= $(CFLAGS) -DENABLE_EXPANSION=1
endif

ifdef CKPT_RECALC
CFLAGS:= $(CFLAGS) -DCKPT_RECALC=$(CKPT_RECALC)
else
CFLAGS:= $(CFLAGS) -DCKPT_RECALC=0
endif

ifdef REPORT
CFLAGS:= $(CFLAGS) -DREPORT=$(REPORT)
else
CFLAGS:= $(CFLAGS) -DREPORT=1
endif
################################# USED FOR SINGLE MODELS################

#PHOLD / PHOLC_COUNT / PHOLD_HOTSPOT
ifdef FAN_OUT
CFLAGS:= $(CFLAGS) -DFAN_OUT=$(FAN_OUT)
endif

#PHOLD / PHOLC_COUNT / PHOLD_HOTSPOT / TCAR /PHOLD_ROLL_MULTICAST / PHOLD_ROLL_BROADCAST
ifdef LOOP_COUNT
CFLAGS:= $(CFLAGS) -DLOOP_COUNT=$(LOOP_COUNT)
endif

#PHOLD_ROLL
ifdef VARIANCE
CFLAGS:= $(CFLAGS) -DVARIANCE=$(VARIANCE)
endif

#PHOLD_ROLL_MUTICAST / PHOLD_ROLL_BROADCAST
ifdef THR_PROB_NORMAL
CFLAGS:= $(CFLAGS) -DTHR_PROB_NORMAL=$(THR_PROB_NORMAL)
endif

#PHOLD_ROLL_NEARS
ifdef NUM_NEARS
CFLAGS:= $(CFLAGS) -DNUM_NEARS=$(NUM_NEARS)
endif

#TCAR
ifdef NUM_CELLE_OCCUPATE
CFLAGS:= $(CFLAGS) -DNUM_CELLE_OCCUPATE=$(NUM_CELLE_OCCUPATE)
endif

#TCAR
ifdef ROBOT_PER_CELLA
CFLAGS:= $(CFLAGS) -DROBOT_PER_CELLA=$(ROBOT_PER_CELLA)
endif

#PHOLD_HOTSPOT
ifdef HOTSPOTS
CFLAGS:= $(CFLAGS) -DHOTSPOTS=$(HOTSPOTS)
endif

#PHOLD_HOTSPOT
ifdef P_HOTSPOT
CFLAGS:= $(CFLAGS) -DP_HOTSPOT=$(P_HOTSPOT)
endif

########################################################################

PCS_PREALLOC_SOURCES=model/pcs-prealloc/application.c\
		    model/pcs-prealloc/functions_app.c\
		    model/pcs-prealloc/topology.c

PCS_HOT_CELLS_SOURCES=model/pcs-hot-cells/application.c\
		    		  model/pcs-hot-cells/functions_app.c

PCS_SOURCES=model/pcs/application.c\
		    model/pcs/functions_app.c

PHOLD_SOURCES=model/phold/application.c

PHOLD_ROLL_MULTICAST_SOURCES=model/phold_roll_multicast/application.c

PHOLD_ROLL_BROADCAST_SOURCES=model/phold_roll_broadcast/application.c

PHOLD_ROLL_NEARS_SOURCES=model/phold_roll_nears/application.c

PHOLD_O_SOURCES=model/phold_o/application.c

PHOLDCOUNT_SOURCES=model/phold_count/application.c

PHOLDHOTSPOT_SOURCES=model/phold_hotspot/application.c

HASH_SOURCES=model/hash/application.c\
			 model/hash/functions.c

TCAR_SOURCES=model/tcar/application.c\
			 model/tcar/neighbours.c

TRAFFIC_SOURCES=model/traffic/application.c\
				model/traffic/functions.c\
				model/traffic/init.c\
				model/traffic/normal_cdf.c

ROBOT_EXPLORE_SOURCES=model/robot_explore/application.c\
		    		  model/robot_explore/neighbours.c


TARGET=phold



ifeq ($(NEW_IPI_SUPPORT),1)
	ASM_SOURCES=core/jmp.S\
		core/trampoline.S\
		core/sync_tramp.S
else
	ifeq ($(NEW_LONG_JMP),1)
	ASM_SOURCES=core/jmp.S
	endif
endif

CORE_SOURCES =  core/ipi_ctrl.c\
		core/core.c\
		core/calqueue.c\
		core/nb_calqueue.c\
		core/x86.c\
		core/topology.c\
		core/queue.c\
		core/fetch.c\
		core/main.c\
		core/numerical.c\
		core/hpdcs_math.c\
		core/parseparam.c\
		statistics/statistics.c\
		core/simple_dynamic_list.c\
		core/atomic_16byte.c\
		core/atomic_epoch_and_ts.c\
		core/posting.c\
		core/checks.c\
		core/ipi.c\
		core/preempt_counter.c\
		core/handle_interrupt.c\
		core/handle_interrupt_with_check.c\
		core/lp_stats.c\
		mm/garbagecollector.c
		

MM_SOURCES=mm/allocator.c\
		mm/dymelor.c\
		mm/recoverable.c\
		mm/checkpoints.c\
		mm/state.c\
		datatypes/list.c\
		mm/platform.c

		
REVERSE_SOURCES=	reverse/reverse.c\
		reverse/slab.c


MM_OBJ=$(MM_SOURCES:.c=.o)
ifeq ($(NEW_LONG_JMP),1)
ASM_OBJ=$(ASM_SOURCES:.S=.o)
endif
CORE_OBJ=$(CORE_SOURCES:.c=.o)
REVERSE_OBJ=$(REVERSE_SOURCES:.c=.o)

PCS_OBJ=$(PCS_SOURCES:.c=.o)
PCS_HOT_CELLS_OBJ=$(PCS_HOT_CELLS_SOURCES:.c=.o)
PCS_PREALLOC_OBJ=$(PCS_PREALLOC_SOURCES:.c=.o)
TRAFFIC_OBJ=$(TRAFFIC_SOURCES:.c=.o)
TCAR_OBJ=$(TCAR_SOURCES:.c=.o)
PHOLD_OBJ=$(PHOLD_SOURCES:.c=.o)
PHOLD_O_OBJ=$(PHOLD_O_SOURCES:.c=.o)
PHOLD_ROLL_MULTICAST_OBJ=$(PHOLD_ROLL_MULTICAST_SOURCES:.c=.o)
PHOLD_ROLL_BROADCAST_OBJ=$(PHOLD_ROLL_BROADCAST_SOURCES:.c=.o)
PHOLD_ROLL_NEARS_OBJ=$(PHOLD_ROLL_NEARS_SOURCES:.c=.o)
PHOLDCOUNT_OBJ=$(PHOLDCOUNT_SOURCES:.c=.o)
PHOLDHOTSPOT_OBJ=$(PHOLDHOTSPOT_SOURCES:.c=.o)
HASH_OBJ=$(HASH_SOURCES:.c=.o)
ROBOT_EXPLORE_OBJ=$(ROBOT_EXPLORE_SOURCES:.c=.o)


all: phold # pcs pcs-prealloc traffic tcar phold robot_explore hash

pcs: TARGET=pcs 
pcs: clean _pcs executable

pcs_hot_cells: TARGET=pcs_hot_cells 
pcs_hot_cells: clean _pcs_hot_cells executable

pcs-prealloc: TARGET=pcs-prealloc 
pcs-prealloc: clean _pcs_prealloc executable

traffic: TARGET=traffic 
traffic: clean _traffic executable

tcar: TARGET=tcar 
tcar: clean _tcar executable

phold: TARGET=phold 
phold: clean  _phold executable

phold_o: TARGET=phold_o
phold_o: clean  _phold_o executable

phold_roll_multicast: TARGET=phold_roll_multicast
phold_roll_multicast: clean _phold_roll_multicast executable

phold_roll_broadcast: TARGET=phold_roll_broadcast
phold_roll_broadcast: clean _phold_roll_broadcast executable

phold_roll_nears: TARGET=phold_roll_nears
phold_roll_nears: clean _phold_roll_nears executable

pholdcount: TARGET=pholdcount 
pholdcount: clean  _pholdcount executable

pholdhotspot: TARGET=pholdhotspot 
pholdhotspot: clean  _pholdhotspot executable

robot_explore: TARGET=robot_explore 
robot_explore: clean _robot_explore executable

hash: TARGET=hash 
hash: clean _hash executable

ifeq ($(NEW_LONG_JMP),1)
executable: mm asm core reverse link
else
executable: mm core reverse link
endif

link:
ifeq ($(REVERSIBLE),1)
	hijacker -c script/hijacker-conf.xml -i model/__application.o -o model/__application_hijacked.o
	#/home/ianni/hijacker_install/bin/hijacker -c script/hijacker-conf.xml -i model/__application.o -o model/__application_hijacked.o
else
	cp model/__application.o model/__application_hijacked.o
endif
ifeq ($(MALLOC),1)
	cp  model/__application_hijacked.o model/application-mm.o
#	ld -r -o model/application.o model/__application_hijacked.o  
#	gcc $(CFLAGS) -o $(TARGET) model/__application_hijacked.o core/__core.o reverse/__reverse.o $(LIBS)
#	ld -r  -o model/application-mm.o model/__application_hijacked.o --whole-archive mm/__mm.o
#	gcc $(CFLAGS) -o $(TARGET) model/__application_hijacked.o core/__core.o $(LIBS)
#	gcc $(CFLAGS) -o $(TARGET) model/__application.o core/__core.o $(LIBS)
else
	ld -r --wrap malloc --wrap free --wrap realloc --wrap calloc -o model/application-mm.o model/__application_hijacked.o --whole-archive mm/__mm.o
#	ld -r --wrap malloc --wrap free --wrap realloc --wrap calloc -o model/application-mm.o model/__application.o --whole-archive mm/__mm.o
#	gcc $(CFLAGS) -o $(TARGET) model/application-mm.o reverse/__reverse.o core/__core.o $(LIBS)
endif

ifeq ($(NEW_LONG_JMP),1)
	gcc $(CFLAGS) -o $(TARGET) model/application-mm.o reverse/__reverse.o core/__asm.o core/__core.o $(LIBS)
else
	gcc $(CFLAGS) -o $(TARGET) model/application-mm.o reverse/__reverse.o  core/__core.o $(LIBS)
endif

mm: $(MM_OBJ)
	@ld -r -g $(MM_OBJ) -o mm/__mm.o

ifeq ($(NEW_LONG_JMP),1)
asm: $(ASM_OBJ)
	@ld -r -g $(ASM_OBJ) -o core/__asm.o
endif

core: $(CORE_OBJ)
	@ld -r -g $(CORE_OBJ) -o core/__core.o

reverse: $(REVERSE_OBJ)
	@ld -r -g $(REVERSE_OBJ) -o reverse/__reverse.o

%.o: %.c
	@echo "[CC] $@"
	@$(CC) -c $(CFLAGS) $(INCLUDE) $< -o $@ $(LIBS)

_pcs_prealloc: $(PCS_PREALLOC_OBJ)
	@ld -r -g $(PCS_PREALLOC_OBJ) -o model/__application.o

_pcs_hot_cells: $(PCS_HOT_CELLS_OBJ)
	@ld -r -g $(PCS_HOT_CELLS_OBJ) -o model/__application.o

_pcs: $(PCS_OBJ)
	@ld -r -g $(PCS_OBJ) -o model/__application.o

_tcar: $(TCAR_OBJ)
	@ld -r -g $(TCAR_OBJ) -o model/__application.o

_phold: $(PHOLD_OBJ)
	@ld -r -g $(PHOLD_OBJ) -o model/__application.o

_phold_o: $(PHOLD_O_OBJ)
	@ld -r -g $(PHOLD_O_OBJ) -o model/__application.o

_phold_roll_multicast: $(PHOLD_ROLL_MULTICAST_OBJ)
	@ld -r -g $(PHOLD_ROLL_MULTICAST_OBJ) -o model/__application.o

_phold_roll_broadcast: $(PHOLD_ROLL_BROADCAST_OBJ)
	@ld -r -g $(PHOLD_ROLL_BROADCAST_OBJ) -o model/__application.o

_phold_roll_nears: $(PHOLD_ROLL_NEARS_OBJ)
	@ld -r -g $(PHOLD_ROLL_NEARS_OBJ) -o model/__application.o

_pholdcount: $(PHOLDCOUNT_OBJ)
	@ld -r -g $(PHOLDCOUNT_OBJ) -o model/__application.o

_pholdhotspot: $(PHOLDHOTSPOT_OBJ)
	@ld -r -g $(PHOLDHOTSPOT_OBJ) -o model/__application.o

_hash: $(HASH_OBJ)
	@ld -r -g $(HASH_OBJ) -o model/__application.o

_traffic: $(TRAFFIC_OBJ)
	@ld -r -g $(TRAFFIC_OBJ) -o model/__application.o

_robot_explore: $(ROBOT_EXPLORE_OBJ)
	@ld -r -g $(ROBOT_EXPLORE_OBJ) -o model/__application.o
	


clean:
	@find . -name "*.o" -exec rm {} \;
	@find . -type f -name "phold" 		         -exec rm {} \;
	@find . -type f -name "phold_roll_multicast" -exec rm {} \;
	@find . -type f -name "phold_roll_broadcast" -exec rm {} \;
	@find . -type f -name "phold_roll_nears"     -exec rm {} \;
	@find . -type f -name "pcs" 		         -exec rm {} \;
	@find . -type f -name "pcs-prealloc"         -exec rm {} \;
	@find . -type f -name "traffic "  	         -exec rm {} \;
	@find . -type f -name "tcar" 		         -exec rm {} \;
	@find . -type f -name "phold" 		         -exec rm {} \;
	@find . -type f -name "phold_o"              -exec rm {} \;
	@find . -type f -name "pholdcount" 	         -exec rm {} \;
	@find . -type f -name "pholdhotspot"         -exec rm {} \;
	@find . -type f -name "robot_explore"        -exec rm {} \;
	@find . -type f -name "hash" 		         -exec rm {} \;
