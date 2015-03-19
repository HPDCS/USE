#Versione test - non uso ottimizzazioni del compilatore

CFLAGS=-Wall -mrtm -pthread
INCLUDE=-I include/

APPLICATION_SOURCES=model/application.c

APP=test

CORE_SOURCES=core/core.c\
		core/queue.c\
		core/main.c\
		core/time_util.c

MM_SOURCES=mm/allocator.c\
		mm/dymelor.c\
		mm/recoverable.c





MM_OBJ=$(MM_SOURCES:.c=.o)
APPLICATION_OBJ=$(APPLICATION_SOURCES:.c=.o)
CORE_OBJ=$(CORE_SOURCES:.c=.o)


all: application mm core
	@echo "Linking"
	ld -r --wrap malloc --wrap free --wrap realloc --wrap calloc model/application.o -o model/application-wrap.o
	ld -r model/application-wrap.o mm/mm.o -o application-mm.o
	gcc -o $(APP) model/application.o core/core.o

mm: $(MM_OBJ)
	@echo "Memory Manager"
	ld -r $(MM_OBJ) -o mm/mm.o

application: $(APPLICATION_OBJ)
	@echo "Application"
	ld -r $(APPLICATION_OBJ) -o mm/mm.o

core: $(CORE_OBJ)
	@echo "Core"
	ld -r $(CORE_OBJ) -o mm/mm.o

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS) $(INCLUDE)


clean:
	find . -name "*.o" -exec rm {} \;
