#ifndef __REVERSE__
#define __REVERSE__

#include <sys/types.h>

#define REVERSE_WIN_SIZE 1024 * 1024 * 50	//! Defalut size of the reverse window which will contain the reverse code
#define HMAP_SIZE		32768	//! Default size ot the address hash map to handle colliding mov addresses

#define HMAP_INDEX_MASK		0xffffffc0	//! Most significant 10 bits are used to index quad-word which contains address bit
#define HMAP_OFFSET_MASK	0x3f	//! Least significant 6 bits are used to intercept address presence
#define HMAP_OFF_MASK_SIZE	6

typedef struct _revwin {
	int size;		//! The actual size of the reverse window
	int flags;		//! Creation flags
	int prot;		//! Creation protection flags
	void *address;		//! Address to which it resides
	void *pointer;		//! Pointer to the new actual free address memory location
} revwin;

// Hash map to handle repeted MOV-targeted addresses
typedef struct _addrmap {
	unsigned long long map[HMAP_SIZE];
} addrmap;

// History of all the reverse windows allocated since the first execution
typedef struct _eras {
	revwin *era[1024];	//! Array of the windows
//      int size;               //! Current size of the history
	int last_free;		//! Index of the last available slot
} eras;

extern __thread revwin *current_revwin;	//! Represents the pointer to the current active reverse window

/**
 * This will allocate a window on the HEAP of the exefutable file in order
 * to write directly on it the reverse code to be executed later on demand.
 *
 * @param size The size to which initialize the new reverse window. If the size paramter is 0, default
 * value will be used (REVERSE_WIN_SIZE)
 *
 * @return The address of the created window
 *
 * Note: mmap is invoked with both write and executable access, but actually
 * it does not to be a good idea since could be less portable and further
 * could open to security exploits.
 */
extern void *create_new_revwin(size_t size);

/**
 * Free the reverse window passed as argument.
 *
 * @param window A pointer to a reverse window
 */
extern void free_revwin(void *window);

extern void execute_undo_event(void *w);

extern void reset_window(void *window);

#endif				//__REVERSE__
