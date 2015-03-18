#Versione test - non uso ottimizzazioni del compilatore


CORE_SOURCES=core/core.c\
		core/queue.c\
		core/main.c\
		core/time_util.c\
		core/application.c
			

all:
	gcc -Wall -o test $(CORE_SOURCES) -mrtm -pthread 
