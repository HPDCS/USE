#Versione test - non uso ottimizzazioni del compilatore

CC=gcc
CFLAGS=-Wall -mrtm -pthread -lm -DTEST_1
INCLUDE=-I include/ -I mm/

APPLICATION_SOURCES=model/application.c

TARGET=test

CORE_SOURCES =  core/src/communication.c\
		core/src/core.c\
		core/src/calqueue.c\
		core/src/main.c\
		core/src/time_util.c\
		core/src/numerical.c

MM_SOURCES=mm/allocator.c\
		mm/dymelor.c\
		mm/recoverable.c





MM_OBJ=$(MM_SOURCES:.c=.o)
APPLICATION_OBJ=$(APPLICATION_SOURCES:.c=.o)
CORE_OBJ=$(CORE_SOURCES:.c=.o)


all: application mm core
	ld -r --wrap malloc --wrap free --wrap realloc --wrap calloc model/__application.o --whole-archive -o model/application-wrap.o
	ld -r model/application-wrap.o mm/__mm.o --whole-archive -o model/application-mm.o
	gcc -o $(APP) model/application-mm.o core/__core.o $(CFLAGS)

mm: $(MM_OBJ)
	@ld -r $(MM_OBJ) -o mm/__mm.o

application: $(APPLICATION_OBJ)
	@ld -r $(APPLICATION_OBJ) -o model/__application.o

core: $(CORE_OBJ)
	@ld -r $(CORE_OBJ) -o core/__core.o

%.o: %.c
	@echo "[CC] $@"
	@$(CC) -c -o $@ $< $(CFLAGS) $(INCLUDE)


clean:
	@find . -name "*.o" -exec rm {} \;
