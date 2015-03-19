#Versione test - non uso ottimizzazioni del compilatore

CFLAGS=-Wall -mrtm -pthread
INCLUDE=-I include/

APPLICATION_SOURCES=model/application.c

APP=test

CFLAGS = -D_TEST_1

TARGET = test_1


CORE_SOURCES = core/core.c\
		core/calqueue.c\
		core/main.c\
		core/time_util.c\
		core/numerical.c

MM_SOURCES=mm/allocator.c\
		mm/dymelor.c\
		mm/recoverable.c





MM_OBJ=$(MM_SOURCES:.c=.o)
APPLICATION_OBJ=$(APPLICATION_SOURCES:.c=.o)
CORE_OBJ=$(CORE_SOURCES:.c=.o)


all: application mm core
	@ld -r --wrap malloc --wrap free --wrap realloc --wrap calloc model/__application.o -o model/application-wrap.o
	@ld -r model/application-wrap.o mm/mm.o -o application-mm.o
	@gcc -o $(APP) model/application-mm.o core/core.o

mm: $(MM_OBJ)
	@ld -r $(MM_OBJ) -o mm/mm.o

application: $(APPLICATION_OBJ)
	@ld -r $(APPLICATION_OBJ) -o model/__application.o

core: $(CORE_OBJ)
	@ld -r $(CORE_OBJ) -o core/core.o

%.o: %.c
	@echo "[CC] $@"
	@$(CC) -c -o $@ $< $(CFLAGS) $(INCLUDE)


clean:
	@find . -name "*.o" -exec rm {} \;
