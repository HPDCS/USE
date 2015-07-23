#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "calqueue.h"

#define P_INSERT	0.8
#define P_INSERT_BACK	0.1
#define P_GET		0.1

#define MAX_TIME	10

static double Rand(void) {
	
	return (double)rand() / (double)RAND_MAX;
}


typedef struct _dummy {
	double time;
} dummy;


int main(int argc, char **argv) {
	int rounds = atoi(argv[1]);
	int i;
	double coin;
	double sum;
	double last_time = 0.0;
	double last_extracted = 0.0;
	dummy *e;
	int total_extractions = 0;
	int total_insertions = 0;
	int total_errors = 0;
	
	srand(time(NULL));
	calqueue_init();
	
	for(i = 0; i < rounds; i++) {
		sum = 0.0;
		coin = Rand();
		
		//~printf("coin is %f\n", coin);
		
		sum += P_INSERT;
		if(coin < sum) {
		    fallback:
			printf("+");
			e = malloc(sizeof(dummy));
			last_time += Rand();
			e->time = last_time;
			calqueue_put(last_time, e);
			total_insertions++;
			continue;
		}
		
		sum += P_INSERT_BACK;
		if(coin < sum) {
			if(last_extracted == 0.0)
				goto fallback;
			printf("-");
			e = malloc(sizeof(dummy));
			coin = Rand();
			if(last_extracted - coin < 0)
				coin = 0.0;
			e->time = last_extracted - coin;
			calqueue_put(e->time, e);
			total_insertions++;
			continue;
		}
		
		e = calqueue_get();
		if(e == NULL)
			continue;
		printf("Get within the insertion loop returned %f\n", e->time);
		total_extractions++;
		last_extracted = e->time;
		free(e);
	}
	
	puts("Starting to remove all elements from calqueue\n");
	last_time = 0.0;
	i = 0;
	
	do {
		e = calqueue_get();
		
		if(e == NULL)
			break;
			
		total_extractions++;
			
		printf("%d: %f ", ++i, e->time);
			
		if(e->time < last_time){ 
			printf("*ERROR*\n"); 
			total_errors++;
		} else
			printf("\n");
		
		last_time = e->time;
		free(e);
	} while(e != NULL);

	printf("TOTAL INSERTIONS: %d\n", total_insertions);
	printf("TOTAL EXTRACTIONS: %d\n", total_extractions);
	printf("TOTAL ERRORS: %d\n", total_errors);
}

