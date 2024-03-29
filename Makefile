#Test version - compilator optimizations disabled

CC=gcc
#FLAGS=-g -Wall -pthread -lm

CFLAGS= -DARCH_X86_64 -g3 -Wall -Wextra -mrtm 

#-DCACHE_LINE_SIZE="getconf LEVEL1_DCACHE_LINESIZE"

#CLS = 64#"getconf LEVEL1_DCACHE_LINESIZE"
CFLAGS:=$(CFLAGS) -DCACHE_LINE_SIZE=$(shell getconf LEVEL1_DCACHE_LINESIZE) -DN_CPU=$(shell grep -c ^processor /proc/cpuinfo)
#-DNUM_NUMA_NODES=$(shell lscpu | grep 'NUMA node(s)' | head -1 | awk '{print $3}')

INCLUDE=-Iinclude/ -Imm/ -Icore/ -Istatistics/ -Ireverse/ -Idatatypes -Iinclude-gen
LIBS=-pthread -lm -lnuma
ARCH_X86=1
ARCH_X86_64=1


ifdef ENFORCE_LOCALITY
CFLAGS:=$(CFLAGS) -DENFORCE_LOCALITY=$(ENFORCE_LOCALITY)
  ifdef ENABLE_DYNAMIC_SIZING_FOR_LOC_ENF
  CFLAGS:=$(CFLAGS) -DENABLE_DYNAMIC_SIZING_FOR_LOC_ENF=$(ENABLE_DYNAMIC_SIZING_FOR_LOC_ENF)
  else
  CFLAGS:=$(CFLAGS) -DENABLE_DYNAMIC_SIZING_FOR_LOC_ENF=1
  endif
else
CFLAGS:=$(CFLAGS) -DENFORCE_LOCALITY=0	
CFLAGS:=$(CFLAGS) -DENABLE_DYNAMIC_SIZING_FOR_LOC_ENF=0
endif


ifdef START_WINDOW
CFLAGS:=$(CFLAGS) -DSTART_WINDOW=$(START_WINDOW)
else
CFLAGS:=$(CFLAGS) -DSTART_WINDOW=0.4
endif

ifdef CURRENT_BINDING_SIZE
CFLAGS:=$(CFLAGS) -DCURRENT_BINDING_SIZE=$(CURRENT_BINDING_SIZE)
endif

ifdef EVICTED_BINDING_SIZE
CFLAGS:=$(CFLAGS) -DEVICTED_BINDING_SIZE=$(EVICTED_BINDING_SIZE)
endif

ifdef MAX_SKIPPED_LP
CFLAGS:=$(CFLAGS) -DMAX_SKIPPED_LP=$(MAX_SKIPPED_LP)
else
CFLAGS:=$(CFLAGS) -DMAX_SKIPPED_LP=100000
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

ifdef NUMA
CFLAGS:=$(CFLAGS) -DHAVE_PARALLEL_ALLOCATOR
CFLAGS:=$(CFLAGS) -DDISTRIBUTED_FETCH=1
CFLAGS:=$(CFLAGS) -DMBIND=1
endif

ifdef PARALLEL_ALLOCATOR
CFLAGS:=$(CFLAGS) -DHAVE_PARALLEL_ALLOCATOR
endif

ifdef DISTRIBUTED_FETCH
CFLAGS:=$(CFLAGS) -DDISTRIBUTED_FETCH=1
endif

ifdef MBIND
CFLAGS:=$(CFLAGS) -DMBIND=1
endif

ifdef LOOKAHEAD
CFLAGS:= $(CFLAGS) -DLOOKAHEAD=$(LOOKAHEAD)
endif

ifdef DEBUG
CFLAGS:= $(CFLAGS) -DDEBUG=$(DEBUG)
else
CFLAGS:= $(CFLAGS) -DDEBUG=0 -DNDEBUG -O3
endif

ifdef REPORT
CFLAGS:= $(CFLAGS) -DREPORT=$(REPORT)
else
CFLAGS:= $(CFLAGS) -DREPORT=1
endif

ifdef LIST_NODE_PER_ALLOC
CFLAGS:= $(CFLAGS) -DLIST_NODE_PER_ALLOC=$(LIST_NODE_PER_ALLOC)
endif

ifdef CKP_PERIOD
CFLAGS:= $(CFLAGS) -DCHECKPOINT_PERIOD=$(CKP_PERIOD)
else
CFLAGS:= $(CFLAGS) -DCHECKPOINT_PERIOD=20
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

ifdef CKPT_RECALC
CFLAGS:= $(CFLAGS) -DCKPT_RECALC=$(CKPT_RECALC)
else
CFLAGS:= $(CFLAGS) -DCKPT_RECALC=0
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

################################# USED FOR SINGLE MODELS################

#PHOLD / PHOLC_COUNT / PHOLD_HOTSPOT
ifdef FAN_OUT
CFLAGS:= $(CFLAGS) -DFAN_OUT=$(FAN_OUT)
endif

#PHOLD / PHOLC_COUNT / PHOLD_HOTSPOT / TCAR
ifdef LOOP_COUNT
CFLAGS:= $(CFLAGS) -DLOOP_COUNT=$(LOOP_COUNT)
endif

ifdef LOOP_COUNT_US
CFLAGS:= $(CFLAGS) -DLOOP_COUNT_US=$(LOOP_COUNT_US)
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

#PCS
ifdef TA_CHANGE
CFLAGS:= $(CFLAGS) -DTA_CHANGE=$(TA_CHANGE)
endif

#PCS
ifdef TA
CFLAGS:= $(CFLAGS) -DTA=$(TA)
endif

#PCS
ifdef TA_DURATION
CFLAGS:= $(CFLAGS) -DTA_DURATION=$(TA_DURATION)
endif

#PCS
ifdef CHANNELS_PER_CELL
CFLAGS:= $(CFLAGS) -DCHANNELS_PER_CELL=$(CHANNELS_PER_CELL)
endif

#TRAFFIC
ifdef ENTER_PROB
CFLAGS:= $(CFLAGS) -DENTER_PROB=$(ENTER_PROB)
endif

#TRAFFIC
ifdef LEAVE_PROB
CFLAGS:= $(CFLAGS) -DLEAVE_PROB=$(LEAVE_PROB)
endif

#TRAFFIC
ifdef SIMPLE_TRAFFIC
CFLAGS:= $(CFLAGS) -DSIMPLE_TRAFFIC=$(SIMPLE_TRAFFIC)
endif

########################################################################

PCS_PREALLOC_SOURCES=model/pcs-prealloc/application.c\
		    model/pcs-prealloc/functions_app.c\
		    model/pcs-prealloc/topology.c


PCS_SOURCES=model/pcs/application.c\
		    model/pcs/functions_app.c

PHOLD_SOURCES=model/phold/application.c

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


NOSQL_SOURCES=model/nosql/application.c

TARGET=phold

CORE_SOURCES =  core/core.c\
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
		core/local_scheduler.c\
		core/local_index/local_index.c\
		core/metrics_for_window.c\
		statistics/statistics.c\
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
CORE_OBJ=$(CORE_SOURCES:.c=.o)
REVERSE_OBJ=$(REVERSE_SOURCES:.c=.o)

PCS_OBJ=$(PCS_SOURCES:.c=.o)
PCS_PREALLOC_OBJ=$(PCS_PREALLOC_SOURCES:.c=.o)
TRAFFIC_OBJ=$(TRAFFIC_SOURCES:.c=.o)
TCAR_OBJ=$(TCAR_SOURCES:.c=.o)
PHOLD_OBJ=$(PHOLD_SOURCES:.c=.o)
PHOLDCOUNT_OBJ=$(PHOLDCOUNT_SOURCES:.c=.o)
PHOLDHOTSPOT_OBJ=$(PHOLDHOTSPOT_SOURCES:.c=.o)
HASH_OBJ=$(HASH_SOURCES:.c=.o)
NOSQL_OBJ=$(NOSQL_SOURCES:.c=.o)
ROBOT_EXPLORE_OBJ=$(ROBOT_EXPLORE_SOURCES:.c=.o)


######################################
# RULES FOR COMPILING MODELS
######################################



all: phold # pcs pcs-prealloc traffic tcar phold robot_explore hash

pcs: TARGET=pcs 
pcs: clean _pcs executable

pcs-prealloc: TARGET=pcs-prealloc 
pcs-prealloc: clean _pcs_prealloc executable

traffic: TARGET=traffic 
traffic: clean _traffic executable

tcar: TARGET=tcar 
tcar: clean _tcar executable

phold: TARGET=phold 
phold: clean  _phold executable

pholdcount: TARGET=pholdcount 
pholdcount: clean  _pholdcount executable

pholdhotspot: TARGET=pholdhotspot 
pholdhotspot: clean  _pholdhotspot executable

robot_explore: TARGET=robot_explore 
robot_explore: clean _robot_explore executable

hash: TARGET=hash 
hash: clean _hash executable


nosql: TARGET=nosql 
nosql: clean _nosql executable

executable: cache_conf mm core reverse link

config: cache_conf

link:
ifeq ($(REVERSIBLE),1)
	#hijacker -c script/hijacker-conf.xml -i model/__application.o -o model/__application_hijacked.o
	/home/ianni/hijacker_install/bin/hijacker -c script/hijacker-conf.xml -i model/__application.o -o model/__application_hijacked.o
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
	gcc $(CFLAGS) -o $(TARGET) model/application-mm.o reverse/__reverse.o core/__core.o $(LIBS)



mm: $(MM_OBJ)
	@ld -r -g $(MM_OBJ) -o mm/__mm.o

core: $(CORE_OBJ)
	@ld -r -g $(CORE_OBJ) -o core/__core.o

cache_conf: include-gen build_scripts/cache.db include-gen/hpipe.h include-gen/clock_constant.h



include-gen:
	mkdir include-gen
	@echo include-gen folder created

include-gen/clock_constant.h:
	cd build_scripts; ./gen_clock_header.sh; mv clock_constant.h ../include-gen


include-gen/hpipe.h:
	cd build_scripts; python3 build_cache_map.py cache.db > hpipe.h; mv hpipe.h ../include-gen
	echo hpipe generated


build_scripts/cache.db:
	cd build_scripts; ./get_cache_data.sh	; cd ..;
	echo cache.db generated



reverse: $(REVERSE_OBJ)
	@ld -r -g $(REVERSE_OBJ) -o reverse/__reverse.o

%.o: %.c
	@echo "[CC] $@ $(CFLAGS)"
	@$(CC) -c $(CFLAGS) $(INCLUDE) $< -o $@ $(LIBS)


_pcs_prealloc: $(PCS_PREALLOC_OBJ)
	@ld -r -g $(PCS_PREALLOC_OBJ) -o model/__application.o

_pcs: $(PCS_OBJ)
	@ld -r -g $(PCS_OBJ) -o model/__application.o

_tcar: $(TCAR_OBJ)
	@ld -r -g $(TCAR_OBJ) -o model/__application.o

_phold: $(PHOLD_OBJ)
	@ld -r -g $(PHOLD_OBJ) -o model/__application.o
	
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

_nosql: $(NOSQL_OBJ)
	@ld -r -g $(NOSQL_OBJ) -o model/__application.o

	


clean:
	@find . -name "*.o" -exec rm {} \;
	@find . -type f -name "phold" 		  -exec rm {} \;
	@find . -type f -name "pcs" 		  -exec rm {} \;
	@find . -type f -name "pcs-prealloc"  -exec rm {} \;
	@find . -type f -name "traffic"  	  -exec rm {} \;
	@find . -type f -name "tcar" 		  -exec rm {} \;
	@find . -type f -name "phold" 		  -exec rm {} \;
	@find . -type f -name "pholdcount" 	  -exec rm {} \;
	@find . -type f -name "pholdhotspot"  -exec rm {} \;
	@find . -type f -name "robot_explore" -exec rm {} \;
	@find . -type f -name "hash" 		  -exec rm {} \;

clean-config:
	-rm include-gen -r


.PHONY: clean clean-config


