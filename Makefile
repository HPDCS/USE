#Versione test - non uso ottimizzazioni del compilatore

CC=gcc
CFLAGS=-g -Wall -mrtm -pthread -lm -DTEST_1
INCLUDE=-I include/ -I mm/ -I core/include/

APPLICATION_SOURCES=model/pcs/application.c\
		    model/pcs/functions_app.c

TARGET=test

CORE_SOURCES =  core/communication.c\
		core/message_state.c\
		core/core.c\
		core/topology.c\
		core/calqueue.c\
		core/main.c\
		core/numerical.c

MM_SOURCES=mm/allocator.c\
		mm/dymelor.c\
		mm/recoverable.c





MM_OBJ=$(MM_SOURCES:.c=.o)
APPLICATION_OBJ=$(APPLICATION_SOURCES:.c=.o)
CORE_OBJ=$(CORE_SOURCES:.c=.o)


all: application mm core
	ld -g -r --wrap malloc --wrap free --wrap realloc --wrap calloc -o model/application-mm.o model/__application.o --whole-archive mm/__mm.o
	gcc -g -o $(TARGET) model/application-mm.o core/__core.o $(CFLAGS)

mm: $(MM_OBJ)
	@ld -r -g $(MM_OBJ) -o mm/__mm.o

application: $(APPLICATION_OBJ)
	@ld -r -g $(APPLICATION_OBJ) -o model/__application.o

core: $(CORE_OBJ)
	@ld -r -g $(CORE_OBJ) -o core/__core.o

%.o: %.c
	@echo "[CC] $@"
	@$(CC) -g -c -o $@ $< $(CFLAGS) $(INCLUDE)


clean:
	@find . -name "*.o" -exec rm {} \;
