#ifndef __PIPE_H_
#define __PIPE_H_


#define PIPE_SIZE 3
#define EVICTED_PIPE_SIZE 4

/* Structures pipe_t and evicted_pipe_t might only differ for their sizes
They are managed as a circular buffer-like structure
lp[]: array of lp indexes
next_to_insert: index of the next position available to insert an lp
last_inserted: most recent lp inserted in the array
size: size of the lp array
*/


typedef struct pipe_t { 

   int lp[PIPE_SIZE];
   int next_to_insert;
   int last_inserted;
   size_t size;

} pipe_t;

typedef struct evicted_pipe_t {

   int lp[EVICTED_PIPE_SIZE];
   int next_to_insert;
   int last_inserted;
   size_t size;

} evicted_pipe_t;




/* initialization of common fields of the two pipe structures */
void init_pipe_generic(size_t size, int lp_array[], int *next_to_insert, int *last_inserted);

void init_pipe(pipe_t *pipe);

void init_evicted_pipe(evicted_pipe_t *pipe);

/*------ Array management functions ------ */


int insert_lp_in_pipe(int lp[], int new_lp, size_t size, int *last_inserted, int *next_to_insert);


#endif