.file	"trampoline.S"
.text

.align 4
.globl cfv_trampoline
.type cfv_trampoline, @function
cfv_trampoline:

#if IPI_POSTING==1

/**************************/
/*     PREAMBLE START     */

	/* save RAX, RBX, RCX, RDX */
	pushq		%rax
	pushq		%rbx
	pushq		%rcx
	pushq		%rdx

	/* save FLAGS */
	lahf
	seto		%al
	pushq		%rax

	/* %rax <--- &rt_dta (type: struct run_time_data *) */
	leaq		rt_data(%rip), %rax

	/* %rbx <--- current_lp (type: unsigned int)  */
	movq		%fs:0, %rcx
	addq		current_lp@gottpoff(%rip), %rcx
	movq		(%rcx), %rbx

	/* %rdx <--- LPS[current_lp] (type: LP_state *) */
	movq		LPS(%rip), %rdx
	movq		(%rdx, %rbx, 8), %rdx

	/* %rcx <--- (&rt_dta)->in_lpstate_best_evt_reliable_offset (type: size_t) */
	/* %rbx <--- LPS[current_lp]->best_evt_reliable (type: msg_t *) */
	/* ( LPS[current_lp]->best_evt_reliable == NULL ) ? */
	movq		(%rax), %rcx
	movq		(%rdx, %rcx, 1), %rbx
	testq		%rbx, %rbx
	jz			.L1

	/* %rcx <--- (&rt_dta)->in_msg_state_offset (type: size_t) */
	/* %rcx <--- LPS[current_lp]->best_evt_reliable->state (type: unsigned int) */
	/* ( LPS[current_lp]->best_evt_reliable->state == ELIMINATED ) ? */
	movq		16(%rax), %rcx
	movq		(%rbx, %rcx, 1), %rcx
	cmpq		$0x2, %rcx
	jz			.L1

	/* %rcx <--- (&rt_dta)->in_msg_tie_breaker_offset (type: size_t) */
	/* %rcx <--- LPS[current_lp]->best_evt_reliable->tie_breaker (type: unsigned int) */
	/* ( LPS[current_lp]->best_evt_reliable->tie_breaker == 0 ) ? */
	movq		24(%rax), %rcx
	movq		(%rbx, %rcx, 1), %rcx
	testq		%rcx, %rcx
	jz			.L1

	/* %rcx <--- (&rt_dta)->in_lpstate_bound_offset (type: size_t) */
	/* %rcx <--- LPS[current_lp]->bound (type: msg_t *) */
	/* ( LPS[current_lp]->bound != NULL ) ? */
	movq		8(%rax), %rcx
	movq		(%rdx, %rcx, 1), %rcx
	testq		%rcx, %rcx
	jnz			.L1

	/* save XMM0 */
	subq		$0x10, %rsp
	movsd		%xmm0, 8(%rsp)

	/* %rdx <--- (&rt_dta)->in_msg_timestamp_offset (type: size_t) */
	/* %xmm0 <--- LPS[current_lp]->bound->timestamp (type: double) */
	/* (%rsp) <--- LPS[current_lp]->bound->timestamp (type: double) */
	/* %xmm0 <--- LPS[current_lp]->best_evt_reliable->timestamp (type: double) */
	/* (  LPS[current_lp]->best_evt_reliable->timestamp >= LPS[current_lp]->bound->timestamp ) ? */
	movq		32(%rax), %rdx
	movsd		(%rcx, %rdx, 1), %xmm0
	movsd		%xmm0, (%rsp)
	movsd		(%rbx, %rdx, 1), %xmm0
	comisd		(%rsp), %xmm0
	jge			.L2

/*      PREAMBLE END      */
/**************************/

#endif

	/* %rsi <--- 1 */
	movq		$0x1, %rsi

	/* %rdi <--- context_buffer */
	movq		%fs:0, %rdi
	addq		cntx_loop@gottpoff(%rip), %rdi

	/* RESTORES CURRENT CONTEXT */
	call		_long_jmp

#if IPI_POSTING==1

/**************************/
/*    POSTAMBLE START     */

.L2:
	/* restore XMM0 */
	movsd		8(%rsp), %xmm0
	addq		$0x10, %rsp

.L1:
	/* New Stack-Frame creation on top
	   of the old Stack-Pointer */
	movq		32(%rsp), %rax
	subq		$32, %rax

	/* Copy content of the Alternate-Stack
	   into the new Stack-Frame */
	movq		(%rsp), %rbx
	movq		%rbx, (%rax)
	movq		8(%rsp), %rbx
	movq		%rbx, 8(%rax)
	movq		16(%rsp), %rbx
	movq		%rbx, 16(%rax)
	movq		24(%rsp), %rbx
	movq		%rbx, 24(%rax)

	/* Stack-Switch occurs here */
	movq		%rax, %rsp

	/* restore FLAGS */
	popq		%rax
	addb		$0x7f, %al
	sahf

	/* restore RBX, RAX, RCX, RDX */
	popq		%rdx
	popq		%rcx
	popq		%rbx
	popq		%rax

/*     POSTAMBLE END      */
/**************************/

#endif

	retq

.size cfv_trampoline, .-cfv_trampoline