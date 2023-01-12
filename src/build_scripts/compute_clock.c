#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/mman.h>
#include <time.h>
#include <pthread.h>
#include <sys/wait.h>
#include <string.h>
#include <string.h>

#define CLOCK_READ() ({ \
			unsigned int lo; \
			unsigned int hi; \
			__asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi)); \
			(unsigned long long)(((unsigned long long)hi) << 32 | lo); \
			})




int main(int argc, char**argv){
	int status, local_pid, i=0;
	unsigned long long exec_time;
	unsigned long long tot_time;
	unsigned long long seconds;
	
	if(argc!=2){
		printf("usage: ./a.out <seconds>\n");
		exit(0);
	}
	seconds=atoi(argv[1]);
	unsigned long long start = CLOCK_READ();
	sleep(seconds);
	tot_time = CLOCK_READ() - start;
	printf("Timer  (clocks): %llu\n",tot_time);
	printf("Timer  (time): %llu\n",seconds);
	printf("Clocks per us: %f\n",tot_time/1000.0/1000.0/seconds);
		
	return 0;
}
