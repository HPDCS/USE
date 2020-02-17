#if LONG_JMP==1
#pragma once
#ifndef __SET_JMP_H__
#define __SET_JMP_H__
#include <preempt_counter.h>
/* Preamble for the *setjmp* function that resolves
   caller-save/non-callee-save mismatch. */
#define set_jmp(cntx_ptr)		({\
								int _set_ret; \
								__asm__ __volatile__ ("pushq %rdi"); \
								_set_ret = _set_jmp(cntx_ptr); \
								__asm__ __volatile__ ("add $8, %rsp"); \
								_set_ret; \
							 })

/* Preamble for the *longjmp* function with the only
   purpose of being compliant to set_jmp. */
//#define long_jmp(cntx_ptr, val)	_long_jmp(cntx_ptr, val)
#if DEBUG==1
#define wrap_long_jmp(cntx_ptr, val) ({\
              if(*preempt_count_ptr!=PREEMPT_COUNT_INIT){\
                printf("invalid preempt_counter value in long_jmp\n");\
                gdb_abort;\
              }\
              _long_jmp(cntx_ptr, val);\
              })
#else
#define wrap_long_jmp(cntx_ptr, val) ({\
              _long_jmp(cntx_ptr, val);\
              })
#endif

#define CFV_INIT 0//must be zero,first time set_jmp returns 0
#define CFV_TO_HANDLE 1
#define CFV_ALREADY_HANDLED 2


/* This structure maintains content of all the CPU registers.  */

typedef struct __context_buffer {
  /* This is the space for general purpose registers. */
  unsigned long long rax;
  unsigned long long rdx;
  unsigned long long rcx;
  unsigned long long rbx;
  unsigned long long rsp;
  unsigned long long rbp;
  unsigned long long rsi;
  unsigned long long rdi;
  unsigned long long r8;
  unsigned long long r9;
  unsigned long long r10;
  unsigned long long r11;
  unsigned long long r12;
  unsigned long long r13;
  unsigned long long r14;
  unsigned long long r15;
  unsigned long long rip;
  unsigned long long flags;
  /* Space for other registers. */
  unsigned char others[512] __attribute__((aligned(16)));
} cntx_buf;

extern long long _set_jmp(cntx_buf *);
extern void _long_jmp(cntx_buf *, long long) __attribute__ ((__noreturn__));

#endif
#endif