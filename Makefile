#Versione test - non uso ottimizzazioni del compilatore


CFLAGS = -DTEST_1

TARGET = test

CORE_SRC_PATH = core/src

SOURCES = $(CORE_SRC_PATH)/core.c\
		$(CORE_SRC_PATH)/calqueue.c\
		$(CORE_SRC_PATH)/communication.c\
		$(CORE_SRC_PATH)/message_state.c\
		$(CORE_SRC_PATH)/main.c\
		$(CORE_SRC_PATH)/time_util.c\
		$(CORE_SRC_PATH)/application.c\

INCLUDE_PATH = core/include

INCLUDE = -I$(INCLUDE_PATH)

all:
	gcc -Wall -o $(TARGET) $(INCLUDE) $(SOURCES) $(CFLAGS) -mrtm -pthread 

