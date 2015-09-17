//#ifdef HAVE_MIXED_SS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/mman.h>
#include <errno.h>

#include <timer.h>

#include "reverse.h"


__thread revwin *current_win = NULL;


// Variables to hold the emulated stack window on the heap
// used to reverse the event without affecting the actual stack;
// This is needed since the reverse-MOVs could overwrite portions
// of what was the function's stack in the past event processing.
__thread void *original_stack = NULL;
__thread void *stack = NULL;


//~static __thread unsigned int revgen_count;	//! ??
static __thread addrmap hashmap;	//! Map of the referenced addresses

/*static void print_bits( long long number )
{
    unsigned long long mask = 0x8000000000000000; // 64 bit
    char digit;
 
    while(mask) {
	digit = ((mask & number) ? '1' : '0');
	putchar(digit);
        mask >>= 1 ;
    }
}*/


/**
 * Writes the reverse instruction passed on the heap reverse window.
 *
 * @param bytes Pointer to the memory region containing the instruction's bytes
 * @param size Instruction's size (in bytes)
 */
static inline void add_reverse_insn(revwin * win, unsigned char *bytes, size_t size) {
	
	//~int i;

	// since the structure is used as a stack, it is needed to create room for the instruction
	win->pointer = (void *)((char *)win->pointer - size);

	// printf("=> Attempt to write on the window heap at <%#08lx> (%d bytes)\n", win->pointer, size);

	if (win->pointer < win->address) {
		fprintf(stderr, "insufficent reverse window memory heap!");
		exit(-ENOMEM);
	}
	// TODO: add timestamp conditional selector
	
	//~for(i = 0; i < size; i++)
		//~printf("%02x ", bytes[i]);
	//~printf("\n");

	// copy the instructions to the heap
	memcpy(win->pointer, bytes, size);
}

static inline void initialize_revwin(revwin * w) {
	unsigned char pop = 0x58;
	unsigned char ret = 0xc3;

	bzero(&hashmap.map, sizeof(hashmap.map));

	// Adds the final RET to the base of the window
	add_reverse_insn(w, &ret, sizeof(ret));
	add_reverse_insn(w, &pop, sizeof(ret));
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
static inline revwin *allocate_reverse_window(size_t size) {

	revwin *win = malloc(sizeof(revwin));
	current_win = win;
	if (win == NULL) {
		perror("Out of memory!");
		abort();
	}

	win->size = size;
	win->address = mmap(NULL, size, PROT_WRITE | PROT_EXEC | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	win->pointer = (void *)((char *)win->address + size - 1);

	if (win->address == MAP_FAILED) {
		perror("mmap failed");
		abort();
	}

	initialize_revwin(win);

	/**
	 * Allocate a new stack window in order to not modify anything
	 * on the real one, otherwise during the reversible undo event
	 * processing, the old stack will overwrite the current one which
	 * collide with the execute_undo_event() function
	 */

	// Allocate 1024 bytes for a limited memory which emulates
	// the future stack of the execute_undo_event()'s stack
	// TODO: da sistemare!!!!
	stack = malloc(EMULATED_STACK_SIZE);
	if(stack == NULL) {
		perror("Out of memory!");
		abort();
	}
	// The stack grows towards low addresses
	stack += (EMULATED_STACK_SIZE - 1); 

	return win;
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
static inline void create_reverse_instruction(revwin * w, void *addr, uint64_t value, size_t size) {
	unsigned char mov[12];	// MOV instruction bytes (at most 9 bytes)
	unsigned char mov2[12];	// second MOV in case of quadword data
	unsigned char movabs[10] = { 0x48, 0xb8 };
	size_t mov_size;
	uint64_t address = (uint64_t) addr;

	uint32_t least_significant_bits;

	// create the MOV to store the destination address movabs $0x11223344aabbccdd,%rax: 48 b8 dd cc bb aa 44 33 22 11
	memcpy(movabs + 2, &address, 8);

	// create the new MOV instruction accordingly to the data size
//	mov[0] = (uint64_t) 0;
	switch (size) {
	case 1:		// BYTE
		// this is the case of the movb $0x00,(%rax): c6 00 00
		mov[0] = 0xc6;
		mov[1] = 0x00;
		//mov[2] = (uint8_t) value;
		memcpy(mov + 2, &value, 1);
		mov_size = 3;
		break;

	case 2:		// WORD
		// this is the case of the movw $0x0000,(%rax): 66 c7 00 00 00
		mov[0] = 0x66;
		mov[1] = 0xc7;
		mov[2] = 0x00;
		//mov[3] = (uint16_t) value;
		memcpy(mov + 3, &value, 2);
		mov_size = 5;
		break;

	case 4:		// LONG-WORD
		// this is the case of the movl $0x000000,(%rax): c7 00 00 00 00 00
		mov[0] = 0xc7;
		mov[1] = 0x00;
		//mov[2] = (uint32_t) value;
		memcpy(mov + 2, &value, 4);
		mov_size = 6;
		break;

	case 8:		// QUAD-WORD
		// this is the case of the movq, however in such a case the immediate cannot be
		// more than 32 bits width; therefore we need two successive mov instructions
		// to transfer the upper and bottom part of the value through the 32 bits immediate
		// movq: 48 c7 00 25  00 00 00 00  00 00 00 00
		mov[0] = mov2[0] = 0xc7;
		mov[1] = mov2[1] = 0x00;

		least_significant_bits = (uint32_t) value;

		//mov[4] = (uint32_t) value;
		memcpy(mov + 2, &least_significant_bits, 4);

		// second part
		least_significant_bits = (value >> 32) & 0x0FFFFFFFF;
		memcpy(mov2 + 2, &least_significant_bits, 4);
		mov_size = 6;
		break;

	default:
		// mmmm....
		abort();
		return;
	}

	// now 'mov' contains the bytes that represents the reverse MOV
	// hence it has to be written on the heap reverse window
	add_reverse_insn(w, mov, mov_size);
	add_reverse_insn(w, movabs, 10);
	if (size == 8) {
		address += 4;
		memcpy(movabs + 2, &address, 8);
		add_reverse_insn(w, mov2, mov_size);
		add_reverse_insn(w, movabs, 10);
	}

	return;
}

/**
 * Check if the address is dirty by looking at the hash map. In case the address
 * is not present adds it and return 0.
 *
 * @param address The address to check
 *
 * @return 1 if the address is present, 0 otherwise
 */
static inline int is_address_referenced(void *address) {
	int idx;
	char offset;

	return 0;
	
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
void reverse_code_generator(void *address, unsigned int size) {
	uint64_t value;
	//~double revgen_time;
	//~timer reverse_instruction_generation;

//	timer_start(reverse_instruction_generation);

	// check if the address is already be referenced, in that case it is not necessary
	// to compute again the reverse instruction, since the former MOV is the one
	// would be restored in future, therefore the subsequent ones are redundant (at least
	// for now...)
	if (is_address_referenced(address)) {
		return;
	}
	// Read the value at specified address
	value = 0;
	switch (size) {
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
	
	//~printf("reverse_code_generator at <%p> %d bytes value %#08lx\n", address, size, value);

	// now the idea is to generate the reverse MOV instruction that will
	// restore the previous value 'value' stored in the memory address
	// based on the operand size selects the proper MOV instruction bytes
	create_reverse_instruction(current_win, address, value, size);
}

void *create_new_revwin(size_t size) {
	revwin *win = allocate_reverse_window(size > 0 ? size : REVERSE_WIN_SIZE);
	return win;
}

void reset_window(void *w) {
	revwin *win = (revwin *) w;
	win->pointer = ((char *)win->address + win->size - 1);
	initialize_revwin(win);
}

void free_revwin(void *w) {
	revwin *win = (revwin *) w;

	if (win == NULL)
		return;

	munmap(win->address, win->size);
	free(win);
	current_win = NULL;
}

void execute_undo_event(void *w) {
	unsigned char push = 0x50;
	revwin *win = (revwin *) w;
	void *revcode;


	add_reverse_insn(w, &push, 1);

	// Temporary swaps the stack pointer to use
	// the emulated one on the heap, instead

	// Save the original stack pointer into 'original_stack'
	// and substitute $RSP the new emulated stack pointer
	__asm__ volatile ("movq %%rsp, %0\n\t"
					  "movq %1, %%rsp" : "=m" (original_stack) : "m" (stack));


	// Calls the reversing function
	revcode = win->pointer;
	((void (*)(void))revcode) ();

	// Replace the original stack on the relative register
	__asm__ volatile ("movq %0, %%rsp" : : "m" (original_stack));


	// Reset the reverse window
	reset_window(w);
}
