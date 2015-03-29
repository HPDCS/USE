#Versione test - non uso ottimizzazioni del compilatore

CC=gcc
CFLAGS=-g -Wall -mrtm -pthread -lm
INCLUDE=-I include/ -I mm/ -I core/include/

PCS_SOURCES=model/pcs/application.c\
		    model/pcs/functions_app.c


PHOLD_SOURCES=model/phold/application.c

TRAFFIC_SOURCES=model/traffic/application.c\
		    model/traffic/functions.c\
		    model/traffic/init.c\
		    model/traffic/normal_cdf.c



TARGET=test

CORE_SOURCES =  core/message_state.c\
		core/core.c\
		core/calqueue.c\
		core/topology.c\
		core/queue.c\
		core/main.c\
		core/numerical.c

MM_SOURCES=mm/allocator.c\
		mm/dymelor.c\
		mm/recoverable.c


MM_OBJ=$(MM_SOURCES:.c=.o)
CORE_OBJ=$(CORE_SOURCES:.c=.o)

PCS_OBJ=$(PCS_SOURCES:.c=.o)
TRAFFIC_OBJ=$(TRAFFIC_SOURCES:.c=.o)
PHOLD_OBJ=$(PHOLD_SOURCES:.c=.o)


all: pcs

pcs: _pcs mm core link

traffic: _traffic mm core link

phold: _phold mm core link


link:
	ld -g -r --wrap malloc --wrap free --wrap realloc --wrap calloc -o model/application-mm.o model/__application.o --whole-archive mm/__mm.o
	gcc -g -o $(TARGET) model/application-mm.o core/__core.o $(CFLAGS)

mm: $(MM_OBJ)
	@ld -r -g $(MM_OBJ) -o mm/__mm.o

core: $(CORE_OBJ)
	@ld -r -g $(CORE_OBJ) -o core/__core.o

%.o: %.c
	@echo "[CC] $@"
	@$(CC) -g -c -o $@ $< $(CFLAGS) $(INCLUDE)

_pcs: $(PCS_OBJ)
	@ld -r -g $(PCS_OBJ) -o model/__application.o

_phold: $(PHOLD_OBJ)
	@ld -r -g $(PHOLD_OBJ) -o model/__application.o

_traffic: $(TRAFFIC_OBJ)
	@ld -r -g $(TRAFFIC_OBJ) -o model/__application.o
	


clean:
	@find . -name "*.o" -exec rm {} \;
