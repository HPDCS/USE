#Versione test - non uso ottimizzazioni del compilatore

CC=gcc
#FLAGS=-g -Wall -pthread -lm

FLAGS= -DARCH_X86_64 -g3 -Wall -Wextra -mrtm -O0 
#-DCACHE_LINE_SIZE="getconf LEVEL1_DCACHE_LINESIZE"

#CLS = 64#"getconf LEVEL1_DCACHE_LINESIZE"
FLAGS:=$(FLAGS) -DCACHE_LINE_SIZE=$(shell getconf LEVEL1_DCACHE_LINESIZE)

INCLUDE=-I include/ -I mm/ -I core/ -Istatistics/
LIBS=-pthread -lm
ARCH_X86=1
ARCH_X86_64=1

ifdef MALLOC
CFLAGS=$(FLAGS) -DNO_DYMELOR
else
CFLAGS=$(FLAGS)
endif

ifdef NBC
CFLAGS:= $(CFLAGS) -DNBC
endif

ifdef REVERSIBLE
CFLAGS:= $(CFLAGS) -DREVERSIBLE=$(REVERSIBLE)
else
CFLAGS:= $(CFLAGS) -DREVERSIBLE=0
endif

ifdef LOOKAHEAD
CFLAGS:= $(CFLAGS) -DLOOKAHEAD=$(LOOKAHEAD)
endif

ifdef FAN_OUT
CFLAGS:= $(CFLAGS) -DFAN_OUT=$(FAN_OUT)
endif

ifdef LOOP_COUNT
CFLAGS:= $(CFLAGS) -DLOOP_COUNT=$(LOOP_COUNT)
endif

ifdef NUM_CELLE_OCCUPATE
CFLAGS:= $(CFLAGS) -DNUM_CELLE_OCCUPATE=$(NUM_CELLE_OCCUPATE)
endif

ifdef ROBOT_PER_CELLA
CFLAGS:= $(CFLAGS) -DROBOT_PER_CELLA=$(ROBOT_PER_CELLA)
endif

ifdef PERC_USED_BUCKET
CFLAGS:= $(CFLAGS) -DPUB=$(PERC_USED_BUCKET)
else
CFLAGS:= $(CFLAGS) -DPUB=0.33
endif

ifdef ELEM_PER_BUCKET
CFLAGS:= $(CFLAGS) -DEPB=$(ELEM_PER_BUCKET)
else
CFLAGS:= $(CFLAGS) -DEPB=3
endif

ifdef SPERIMENTAL
CFLAGS:= $(CFLAGS) -DSPERIMENTAL=$(SPERIMENTAL)
else
CFLAGS:= $(CFLAGS) -DSPERIMENTAL=0
endif

ifdef DEBUG
CFLAGS:= $(CFLAGS) -DDEBUG=$(DEBUG)
else
CFLAGS:= $(CFLAGS) -DDEBUG=1
endif

ifdef REPORT
CFLAGS:= $(CFLAGS) -DREPORT=$(REPORT)
else
CFLAGS:= $(CFLAGS) -DREPORT=1
endif

ifdef PREEMPTIVE
CFLAGS:= $(CFLAGS) -DPREEMPTIVE=$(PREEMPTIVE)
else
CFLAGS:= $(CFLAGS) -DPREEMPTIVE=0
endif




PCS_PREALLOC_SOURCES=model/pcs-prealloc/application.c\
		    model/pcs-prealloc/functions_app.c\
		    model/pcs-prealloc/topology.c


PCS_SOURCES=model/pcs/application.c\
		    model/pcs/functions_app.c

PHOLD_SOURCES=model/phold/application.c

PHOLDCOUNT_SOURCES=model/phold_count/application.c

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


TARGET=test

CORE_SOURCES =  core/core.c\
		core/calqueue.c\
		core/nb_calqueue.c\
		core/x86.c\
		core/topology.c\
		core/queue.c\
		core/main.c\
		core/numerical.c\
		core/hpdcs_math.c\
		statistics/statistics.c
#		mm/reverse.c\
#		mm/slab.c

MM_SOURCES=mm/allocator.c\
		mm/dymelor.c\
		mm/recoverable.c
		
REVERSE_SOURCES=	mm/reverse.c\
		mm/slab.c


MM_OBJ=$(MM_SOURCES:.c=.o)
CORE_OBJ=$(CORE_SOURCES:.c=.o)
REVERSE_OBJ=$(REVERSE_SOURCES:.c=.o)

PCS_OBJ=$(PCS_SOURCES:.c=.o)
PCS_PREALLOC_OBJ=$(PCS_PREALLOC_SOURCES:.c=.o)
TRAFFIC_OBJ=$(TRAFFIC_SOURCES:.c=.o)
TCAR_OBJ=$(TCAR_SOURCES:.c=.o)
PHOLD_OBJ=$(PHOLD_SOURCES:.c=.o)
PHOLDCOUNT_OBJ=$(PHOLDCOUNT_SOURCES:.c=.o)
HASH_OBJ=$(HASH_SOURCES:.c=.o)
ROBOT_EXPLORE_OBJ=$(ROBOT_EXPLORE_SOURCES:.c=.o)

all:
	echo $(CFLAGS)
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

robot_explore: TARGET=robot_explore 
robot_explore: clean _robot_explore executable

hash: TARGET=hash 
hash: clean _hash executable

executable: mm core reverse link


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
	gcc $(CFLAGS) -o $(TARGET) model/application-mm.o reverse/__reverse.o core/__core.o $(LIBS)



mm: $(MM_OBJ)
	@ld -r -g $(MM_OBJ) -o mm/__mm.o

core: $(CORE_OBJ)
	@ld -r -g $(CORE_OBJ) -o core/__core.o

reverse: $(REVERSE_OBJ)
	@ld -r -g $(REVERSE_OBJ) -o reverse/__reverse.o

%.o: %.c
	@echo "[CC] $@"
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

_hash: $(HASH_OBJ)
	@ld -r -g $(HASH_OBJ) -o model/__application.o

_traffic: $(TRAFFIC_OBJ)
	@ld -r -g $(TRAFFIC_OBJ) -o model/__application.o

_robot_explore: $(ROBOT_EXPLORE_OBJ)
	@ld -r -g $(ROBOT_EXPLORE_OBJ) -o model/__application.o
	


clean:
	@find . -name "*.o" -exec rm {} \;
	@find . -type f -name "phold" 		  -exec rm {} \;
	@find . -type f -name "pcs" 		  -exec rm {} \;
	@find . -type f -name "pcs-prealloc"  -exec rm {} \;
	@find . -type f -name "traffic "  	  -exec rm {} \;
	@find . -type f -name "tcar" 		  -exec rm {} \;
	@find . -type f -name "phold" 		  -exec rm {} \;
	@find . -type f -name "robot_explore" -exec rm {} \;
	@find . -type f -name "hash" 		  -exec rm {} \;
	
 		 
