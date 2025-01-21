CC = gcc 
CFLAGS = -Wall -Wextra -I ../../include/ -I ./ -c -lm -Wimplicit-fallthrough $(_FLAGS) $(_HWSETUP)
LINKER = -Xlinker --wrap=malloc 
TARGET1 = intermediate-pcs1.o 
TARGET2 =  
TARGET3 = intermediate-pcs3.o 

SRCS1 = application.c 
#SRCS2 = functions-app.c 
SRCS2 =  
SRCS3 = topology.c 

all: 
	$(CC) $(SRCS1) $(CFLAGS) -o $(TARGET1) $(LINKER)
	$(CC) $(SRCS3) $(CFLAGS) -o $(TARGET3) $(LINKER)
	ld -r $(TARGET1) $(TARGET2) $(TARGET3) --wrap=malloc --wrap=free -o pcs.o

clean:
	$(RM) *.o $(TARGET)
