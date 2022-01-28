#include <stdio.h>
#include <stdlib.h>


#include "pipe.h"


/* The function initializes common fields of the two structures
@param: size The size of the pipe
@param: lp_array The array of lp 
@param: next_to_insert The index of the next to be inserted element
@param: last_inserted The index of the last inserted element into the pipe
*/
void init_pipe_generic(size_t size, int lp_array[], int *next_to_insert, int *last_inserted) {

	for (int i = 0; i < size; i++) {
		lp_array[i] = -1;
	}
	*next_to_insert = *last_inserted = 0;
	
}

/* The function initializes the pipe
@param: pipe The pipe
*/
void init_pipe(pipe_t *pipe) {

	pipe->size = PIPE_SIZE;

	init_pipe_generic(pipe->size, pipe->lp, &pipe->next_to_insert, &pipe->last_inserted);

}


/* The function initializes the evicted pipe
@param: pipe The evicted pipe
Written to support pipe of different sizes
*/
void init_evicted_pipe(evicted_pipe_t *pipe) { 

	pipe->size = EVICTED_PIPE_SIZE;

	init_pipe_generic(pipe->size, pipe->lp, &pipe->next_to_insert, &pipe->last_inserted);

}


/* The function inserts an lp into a pipe, might be the evicted pipe too
@param: lp The array of lp
@param: new_lp The new lp to insert
@param: size The size of the pipe
@param last_inserted The index of the most recent element inserted into the pipe
@param next_to_insert The index of the next element to insert
It return the index of an element to be evicted, or -1 if no eviction must take place



If lp_to_evict is >= 0 this function can be called to perform an eviction, where lp is the array
of the evicted pipe and new_lp is lp_to_evict
*/
int insert_lp_in_pipe(int lp[], int new_lp, size_t size, int *last_inserted, int *next_to_insert) {

	int lp_to_evict = -1;

	if (*last_inserted == *next_to_insert) { //empty pipe 
		lp[*next_to_insert] = new_lp;
		*next_to_insert = (*next_to_insert + 1) % size;
		return -1;
	} 
	

	if (lp[*next_to_insert] != -1) { lp_to_evict = lp[*next_to_insert]; } //detect lp to be evicted
		  

	lp[*next_to_insert] = new_lp; 
	

	*last_inserted = *next_to_insert;
	*next_to_insert = (*next_to_insert + 1) % size;
	

	return lp_to_evict;
}


