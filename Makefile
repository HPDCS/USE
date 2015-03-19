#Versione test - non uso ottimizzazioni del compilatore


CFLAGS = -D_TEST_1

TARGET = test_1


CORE_SOURCES = core/core.c\
		core/queue.c\
		core/main.c\
		core/time_util.c\
		core/application.c
			

all:
	gcc -Wall -o $(TARGET) $(CORE_SOURCES) $(CFLAGS) -mrtm -pthread 
