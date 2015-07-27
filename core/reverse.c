//#ifdef HAVE_MIXED_SS


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/mman.h>
#include <errno.h>

#include <timer.h>

#include "reverse.h"

__thread revwin * current_revwin;			//! Pointer to the current reversion window

static __thread unsigned int revgen_count;	//! ??
static __thread addrmap hashmap;			//! Map of the referenced addresses


/**
 * Writes the reverse instruction passed on the heap reverse window.
 *
 * @param bytes Pointer to the memory region containing the instruction's bytes
 * @param size Instruction's size (in bytes)
 */
static inline void add_reverse_insn (char *bytes, size_t size) {

	// since the structure is used as a stack, it is needed to create room for the instruction
	current_revwin->pointer = (void *)((char *)current_revwin->pointer - size);

	// printf("=> Attempt to write on the window heap at <%#08lx> (%d bytes)\n", current_revwin->pointer, size);

	if (current_revwin->pointer < current_revwin->address) {
		perror("insufficent reverse window memory heap!");
		exit(-ENOMEM);
	}

	// TODO: add timestamp conditional selector

	// copy the instructions to the heap
	memcpy(current_revwin->pointer, bytes, size);
}


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
static inline revwin *allocate_reverse_window (size_t size) {
	char pop = 0x58;
	char ret = 0xc3;

	current_revwin = malloc(sizeof(revwin));
	if(current_revwin == NULL) {
		perror("Out of memroy!\n");
		abort();
	}

	current_revwin->size = size;
	current_revwin->address = mmap(NULL, size, PROT_WRITE | PROT_EXEC | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	current_revwin->pointer = (void *)((char *)current_revwin->address + size - 1);


	if(current_revwin->address == MAP_FAILED) {
		perror("mmap failed");
		abort();
	}

	// TODO: move it out?
	bzero(&hashmap.map, sizeof(hashmap.map));


	// Adds the final RET to the base of the window
	add_reverse_insn(&ret, sizeof(ret));
	add_reverse_insn(&pop, sizeof(ret));

	// TODO: modificare la gestione di last_free come negli unix file descriptor
	// check if the entry is empty, if this is the case then fill it up with the new
	// window, otherwise it aborts. Note: the last free slot pointer will be incremented

	return current_revwin;
}


/**
 * Private function used to create the new reversed MOV instruction and
 * manipulate properly the additional information (i.e the timestamp)
 * accordingly to maintain consistency in the reverse code.
 *
 * The timestamp is used to jump the uneeded instructions, hence the ones
 * which have not to be reversed.
 *
 * The revesre instructions have to be written into the reverse window structure
 * in reverse order, too. That is, starting from the bottom of the structure.
 * In this way, it is simply needed to call a certain instruction address to
 * reverse all the subsequant instructions using the IP natural increment.
 *
 * @param value Tha immediate value has to be emdedded into the instruction
 * @param size The size of the data value emdedded
 */
static inline void create_reverse_instruction (uint64_t address, uint64_t value, size_t size) {
	char mov[12];		// MOV instruction bytes (at most 9 bytes)
	char mov2[12];		// second MOV in case of quadword data
	size_t mov_size;


//	printf("creating reverse instruction: address= %llx, value= %llx, size= %d\n", address, value, size);

	// create the MOV to store the destination address movabs $0x11223344aabbccdd,%rax: 48 b8 dd cc bb aa 44 33 22 11
	mov[0] = 0x48;
	mov[1] = 0xb8;
	memcpy(mov+2, &address, 8);
	add_reverse_insn(mov, 10);

	// create the new MOV instruction accordingly to the data size
	mov[0] = (uint64_t) 0;
	switch(size) {
		case 1:	// BYTE
			// this is the case of the movb $0x00,(%rax): c6 00 00
			mov[0] = 0xc6;
			mov[1] = 0x00;
			//mov[2] = (uint8_t) value;
			memcpy(mov+2, &value, 1);
			mov_size = 3;
			break;

		case 2:	// WORD
			// this is the case of the movw $0x0000,(%rax): 66 c7 00 00 00
			mov[0] = 0x66;
			mov[1] = 0xc7;
			mov[2] = 0x00;
			//mov[3] = (uint16_t) value;
			memcpy(mov+3, &value, 2);
			mov_size = 5;
			break;

		case 4:	// LONG-WORD
			// this is the case of the movl $0x000000,(%rax): c7 00 00 00 00 00
			mov[0] = 0xc7;
			mov[1] = 0x00;
			//mov[2] = (uint32_t) value;
			memcpy(mov+2, &value, 4);
			mov_size = 6;
			break;

		case 8:	// QUAD-WORD
			// this is the case of the movq, however in such a case the immediate cannot be
			// more than 32 bits width; therefore we need two successive mov instructions
			// to transfer the upper and bottom part of the value through the 32 bits immediate
			// movq: 48 c7 00 25  00 00 00 00  00 00 00 00
			mov[0] = mov2[0] = 0x48;
			mov[1] = mov2[1] = 0xc7;
			mov[2] = mov2[2] = 0x00;
			//mov[4] = (uint32_t) value;
			memcpy(mov+3, &value, 4);

			// second part
			//mov2[4] = (uint32_t) (value >> 32);
			value = value >> 32;
			memcpy(mov2+3, &value, 4);
			mov_size = 7;
			break;

		default:
			// mmmm....
			return;
		}

	// now 'mov' contains the bytes that represents the reverse MOV
	// hence it has to be written on the heap reverse window
	add_reverse_insn(mov, mov_size);
	if (size == 8) {
		add_reverse_insn(mov2, mov_size);
	}
}


/**
 * Check if the address is dirty by looking at the hash map. In case the address
 * is not present adds it and return 0.
 *
 * @param address The address to check
 *
 * @return 1 if the address is present, 0 otherwise
 */
static inline int is_address_referenced (void *address) {
	int idx;
	char offset;

	// TODO: how to handle full map?

	idx = ((unsigned long long)address & HMAP_INDEX_MASK) >> HMAP_OFF_MASK_SIZE;
	offset = (unsigned long long)address & HMAP_OFFSET_MASK;

	// Closure
	idx %= HMAP_SIZE;
	offset %= 64;

	// if the address is not reference yet, insert it and return 0 (false)
	if ((hashmap.map[idx] >> offset) == 0) {
		hashmap.map[idx] |= (1 << offset);
		return 0;
	}

	return 1;
}


/**
 * Genereate the reverse MOV instruction staring from the knowledge of which
 * memory address will be accessed and the data width of the write.
 *
 * @param address The pointer to the memeory location which the MOV refers to
 * @param size The size of data will be written by MOV
 */
void reverse_code_generator (void *address, unsigned int size) {
	uint64_t value;
	double revgen_time;
	timer reverse_instruction_generation;

	//printf("reverse_code_generator at <%#08llx> (%d bytes)\n", address, size);

	timer_start(reverse_instruction_generation);

	// check if the address is already be referenced, in that case it is not necessary
	// to compute again the reverse instruction, since the former MOV is the one
	// would be restored in future, therefore the subsequent ones are redundant (at least
	// for now...)
	if (is_address_referenced(address)) {
		return;
	}

	// Read the value at specified address
	value = 0;
	switch(size) {
		case 1:
			value = *((uint8_t *) address);
			break;

		case 2:
			value = *((uint16_t *) address);
			break;

		case 4:
			value = *((uint32_t *) address);
			break;

		case 8:
			value = *((uint64_t *) address);
			break;
	}


	// now the idea is to generate the reverse MOV instruction that will
	// restore the previous value 'value' stored in the memory address
	// based on the operand size selects the proper MOV instruction bytes
	create_reverse_instruction(address, value, size);
}


// Library functions
/*
inline void trampoline_initialize () {
	if(current_era > 0) return;

	//history.size = sizeof(history.era);
	window->prot = PROT_EXEC | PROT_READ;		//TODO: change into WRITE, add the on-the-fly EXEC switch mechanism
	window->flags = MAP_PRIVATE | MAP_ANONYMOUS;

	//history.size = sizeof(history.era);
	increment_era();
}


inline int increment_era () {

	last_era++;
	allocate_reverse_window(REVERSE_WIN_SIZE);

	return last_era;
}


inline void free_last_revwin () {
	revwin *window;

	history.last_free--;
	window = history.era[history.last_free];
	munmap(window->address, window->size);
	history.era[history.last_free] = NULL;
}
*/
void *create_new_revwin(size_t size) {

	current_revwin = allocate_reverse_window(size > 0 ? size : REVERSE_WIN_SIZE);

	return current_revwin;
}

void free_revwin (void *w) {
	revwin *win = (revwin *)w;

	if(win == NULL)
		return;

	munmap(win->address, win->size);
	free(win);
}

void execute_undo_event(void *w) {
	revwin *win = (revwin *)w;
	void *revcode;
//	int ret;

	//TODO: gestione dei permessi di esecuzione
/*	ret = mprotect(revcode, window->size, PROT_EXEC);
	if(ret < 0) {
		perror("Error on giving executable grant to current revwin\n");
		abort();
	}
*/
	// Esecuzione della revwin
	// TODO: è necessario anche salvare i valori dei registri, prima?
//	char pushad = 0x60;
//	add_reverse_insn(pushad, 1);

	revcode = win->pointer;
	((void(*)(void))revcode)();

	///mauro: potremmo svuotare il thread pool qui

	//TODO: gestione dei permessi di esecuzione
/*	ret = mprotect(revcode, window->size, PROT_WRITE);
	if(ret < 0) {
		perror("Error on giving write grant to current revwin\n");
		abort();
	}
*/
}

void finalize_revwin(void) {
	char push = 0x50;
	add_reverse_insn(&push, 1);
}

//#endif /* HAVE_MIXED_SS */


