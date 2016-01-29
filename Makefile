#Versione test - non uso ottimizzazioni del compilatore

CC=gcc
#FLAGS=-g -Wall -pthread -lm
FLAGS=-g3 -Wall -Wextra -mrtm
INCLUDE=-I include/ -I mm/ -I core/ -Istatistics/
LIBS=-pthread -lm


ifdef MALLOC
CFLAGS=$(FLAGS) -DNO_DYMELOR
else
CFLAGS=$(FLAGS)
endif


PCS_PREALLOC_SOURCES=model/pcs-prealloc/application.c\
		    model/pcs-prealloc/functions_app.c\
		    model/pcs-prealloc/topology.c


PCS_SOURCES=model/pcs/application.c\
		    model/pcs/functions_app.c


PHOLD_SOURCES=model/phold/application.c

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
		core/topology.c\
		core/queue.c\
		core/main.c\
		core/numerical.c\
		statistics/statistics.c

MM_SOURCES=mm/allocator.c\
		mm/dymelor.c\
		mm/recoverable.c\
		mm/reverse.c\
		mm/slab.c


MM_OBJ=$(MM_SOURCES:.c=.o)
CORE_OBJ=$(CORE_SOURCES:.c=.o)

PCS_OBJ=$(PCS_SOURCES:.c=.o)
PCS_PREALLOC_OBJ=$(PCS_PREALLOC_SOURCES:.c=.o)
TRAFFIC_OBJ=$(TRAFFIC_SOURCES:.c=.o)
TCAR_OBJ=$(TCAR_SOURCES:.c=.o)
PHOLD_OBJ=$(PHOLD_SOURCES:.c=.o)
ROBOT_EXPLORE_OBJ=$(ROBOT_EXPLORE_SOURCES:.c=.o)


all: phold

pcs: _pcs mm core link

pcs-prealloc: _pcs_prealloc mm core link

traffic: _traffic mm core link

tcar: _tcar mm core link

phold: _phold mm core link

robot_explore: _robot_explore mm core link


link:
	hijacker -c script/hijacker-conf.xml -i model/__application.o -o model/__application_hijacked.o
ifdef MALLOC
	gcc $(CFLAGS) -o $(TARGET) model/__application_hijacked.o core/__core.o
#	gcc $(CFLAGS) -o $(TARGET) model/__application.o core/__core.o $(LIBS)
else
	ld -r --wrap malloc --wrap free --wrap realloc --wrap calloc -o model/application-mm.o model/__application_hijacked.o --whole-archive mm/__mm.o
#	ld -r --wrap malloc --wrap free --wrap realloc --wrap calloc -o model/application-mm.o model/__application.o --whole-archive mm/__mm.o
	gcc $(CFLAGS) -o $(TARGET) model/application-mm.o core/__core.o $(LIBS)
endif


mm: $(MM_OBJ)
	@ld -r -g $(MM_OBJ) -o mm/__mm.o

core: $(CORE_OBJ)
	@ld -r -g $(CORE_OBJ) -o core/__core.o

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

_traffic: $(TRAFFIC_OBJ)
	@ld -r -g $(TRAFFIC_OBJ) -o model/__application.o

_robot_explore: $(ROBOT_EXPLORE_OBJ)
	@ld -r -g $(ROBOT_EXPLORE_OBJ) -o model/__application.o
	


clean:
	@find . -name "*.o" -exec rm {} \;
