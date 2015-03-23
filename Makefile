#Versione test - non uso ottimizzazioni del compilatore


CFLAGS =

TARGET = test


CORE_SOURCES = core/core.c\
		core/queue.c\
		core/main.c\
		core/time_util.c\
		core/application.c\
		core/message_state.c
			

all:
	gcc -Wall -o $(TARGET) $(CORE_SOURCES) $(CFLAGS) -mrtm -pthread 
