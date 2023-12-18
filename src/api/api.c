#include <lp/lp.h>
#include <queue.h>
#include <core.h>
#include <lp/mm/allocator/dymelor.h>

extern LP_state **LPS;
extern malloc_state **recoverable_state;

unsigned int GetNumaNode(unsigned int lp){return LPS[lp]->numa_node;}

// can be replaced with a macro?
void ScheduleNewEvent(unsigned int receiver, simtime_t timestamp, unsigned int event_type, void *event_content, unsigned int event_size) {
	if(LPS[current_lp]->state != LP_STATE_SILENT_EXEC){
		queue_insert(receiver, timestamp, event_type, event_content, event_size);
	}
}


/**
* This is the wrapper of the real stdlib malloc. Whenever the application level software
* calls malloc, the call is redirected to this piece of code which uses the memory preallocated
* by the DyMeLoR subsystem for serving the request. If the memory in the malloc_area is exhausted,
* a new one is created, relying on the stdlib malloc.
* In future releases, this wrapper will be integrated with the Memory Management subsystem,
* which is not yet ready for production.
*
* @author Roberto Toccaceli
* @author Francesco Quaglia
*
* @param size Size of the allocation
* @return A pointer to the allocated memory
*
*/
void *__wrap_malloc(size_t size) {
	void *ptr;
	unsigned int numa_node = numa_from_lp(current_lp);
	ptr = do_malloc(current_lp, recoverable_state[current_lp], size, numa_node);

	return ptr;
}

/**
* This is the wrapper of the real stdlib free. Whenever the application level software
* calls free, the call is redirected to this piece of code which will set the chunk in the
* corresponding malloc_area as not allocated.
*
* For further information, please see the paper:
* 	R. Toccaceli, F. Quaglia
* 	DyMeLoR: Dynamic Memory Logger and Restorer Library for Optimistic Simulation Objects
* 	with Generic Memory Layout
*	Proceedings of the 22nd Workshop on Principles of Advanced and Distributed Simulation
*	2008
*
* @author Roberto Toccaceli
* @author Francesco Quaglia
*
* @param ptr A memory buffer to be free'd
*
*/
void __wrap_free(void *ptr) {
	do_free(current_lp, recoverable_state[current_lp], ptr);
}



/**
* This is the wrapper of the real stdlib realloc. Whenever the application level software
* calls realloc, the call is redirected to this piece of code which rely on wrap_malloc
*
* @author Roberto Vitali
*
* @param ptr The pointer to be buffer to be reallocated
* @param size The size of the allocation
* @return A pointer to the newly allocated buffer
*
*/
void *__wrap_realloc(void *ptr, size_t size){

	void *new_buffer;
	size_t old_size;
	malloc_area *m_area;

	// If ptr is NULL realloc is equivalent to the malloc
	if (ptr == NULL) {
		return __wrap_malloc(size);
	}

	// If ptr is not NULL and the size is 0 realloc is equivalent to the free
	if (size == 0) {
		__wrap_free(ptr);
		return NULL;
	}

	m_area = get_area(ptr);

	// The size could be greater than the real request, but it does not matter since the realloc specific requires that
	// is copied at least the smaller buffer size between the new and the old one
	old_size = m_area->chunk_size;

	new_buffer = __wrap_malloc(size);

	if (new_buffer == NULL)
		return NULL;

	memcpy(new_buffer, ptr, size > old_size ? size : old_size);
	__wrap_free(ptr);

	return new_buffer;
}



/**
* This is the wrapper of the real stdlib calloc. Whenever the application level software
* calls calloc, the call is redirected to this piece of code which relies on wrap_malloc
*
* @author Roberto Vitali
*
* @param size The size of the allocation
* @return A pointer to the newly allocated buffer
*
*/
void *__wrap_calloc(size_t nmemb, size_t size){

	void *buffer;

	if (nmemb == 0 || size == 0)
		return NULL;

	buffer = __wrap_malloc(nmemb * size);
	if (buffer == NULL)
		return NULL;

	bzero(buffer, nmemb * size);

	return buffer;
}
