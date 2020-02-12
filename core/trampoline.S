.file	"trampoline.S"
.text

.macro countoccurrences varname
	pushq		%rsi
	pushq		%rdi
	movq		%fs:0, %rsi
	addq		\varname@gottpoff(%rip), %rsi
	movq		(%rsi), %rdi
	addq		$1, %rdi
	movq		%rdi, (%rsi)
	popq		%rdi
	popq		%rsi
.endm

.align 4
.globl cfv_trampoline
.type cfv_trampoline, @function
cfv_trampoline:

/**************************/
/*     PREAMBLE START     */

	/* save RAX, RBX */
	pushq		%rax
	pushq		%rbx

	/* save FLAGS */
	lahf
	seto		%al
	pushq		%rax

	/* save RCX, RDX */
	pushq		%rcx
	pushq		%rdx

	/* save XMM0, XMM1 */
	subq		$0x20, %rsp
	movdqu		%xmm0, 16(%rsp)
	movdqu		%xmm1, (%rsp)

	/* %rax <--- &rt_data (type: struct run_time_data *) */
	leaq		rt_data(%rip), %rax

	/* %rbx <--- tid (type: unsigned int)  */
	movq		%fs:0, %rcx
	addq		tid@gottpoff(%rip), %rcx
	movq		(%rcx), %rbx

	/* %rdx <--- thread_stats (type: LP_state *) */
	/* %rcx <--- (&rt_data)->sizeof_stats (type: size_t)  */
	/* %rcx <--- tid * (&rt_data)->sizeof_stats (type: size_t)  */
	/* %rdx <--- &thread_stats[tid] (type: LP_state *) */
	movq		thread_stats(%rip), %rdx
	movq		48(%rax), %rcx
	imulq		%rbx, %rcx
	addq		%rcx, %rdx

	/* %rcx <--- (&rt_data)->in_stats_ipi_trampoline_received_offset (type: size_t)  */
	/* %xmm0 <--- (&thread_stats[tid])->ipi_trampoline_received (type: double) */
	/* %xmm1 <--- 1.0 (type: double) */
	/* %xmm0 <--- (&thread_stats[tid])->ipi_trampoline_received + 1.0 (type: double) */
	/* %rcx <--- (&rt_data)->in_stats_ipi_trampoline_received_offset (type: size_t)  */
	/* (&thread_stats[tid])->ipi_trampoline_received <--- (&thread_stats[tid])->ipi_trampoline_received + 1.0 (type: double) */
	movq		40(%rax), %rcx
	movsd		(%rdx, %rcx, 1), %xmm0
	movq		$0x1, %rcx
	cvtsi2sd	%rcx, %xmm1
	addsd		%xmm1, %xmm0
	movq		40(%rax), %rcx
	movsd		%xmm0, (%rdx, %rcx, 1)

	/* %rbx <--- current_lp (type: unsigned int)  */
	movq		%fs:0, %rcx
	addq		current_lp@gottpoff(%rip), %rcx
	movq		(%rcx), %rbx

	/* %rdx <--- LPS[current_lp] (type: LP_state *) */
	movq		LPS(%rip), %rdx
	movq		(%rdx, %rbx, 8), %rdx

	countoccurrences trampoline_count1

	/* %rcx <--- (&rt_data)->in_lpstate_priority_message_offset (type: size_t) */
	/* %rbx <--- LPS[current_lp]->priority_message (type: msg_t *) */
	/* ( LPS[current_lp]->priority_message == NULL ) ? */
	movq		(%rax), %rcx
	movq		(%rdx, %rcx, 1), %rbx
	testq		%rbx, %rbx
	jz			.L1

	countoccurrences trampoline_count2

	/* %rcx <--- (&rt_data)->in_msg_state_offset (type: size_t) */
	/* %rcx <--- LPS[current_lp]->priority_message->state (type: unsigned int) */
	/* ( LPS[current_lp]->priority_message->state == ANTI_MSG ) ? */
	movq		16(%rax), %rcx
	movq		(%rbx, %rcx, 1), %rcx
	cmpq		$0x3, %rcx
	jnz			.L2

	countoccurrences trampoline_count3

	/* %rcx <--- (&rt_data)->in_msg_monitor_offset (type: size_t) */
	/* %rcx <--- LPS[current_lp]->priority_message->monitor (type: unsigned int) */
	/* ( LPS[current_lp]->priority_message->monitor == 0xBA4A4A ) ? */
	movq		24(%rax), %rcx
	movq		(%rbx, %rcx, 1), %rcx
	cmpq		$0xBA4A4A, %rcx
	jz			.L1

	countoccurrences trampoline_count4

.L2:
	/* %rcx <--- (&rt_data)->in_msg_state_offset (type: size_t) */
	/* %rcx <--- LPS[current_lp]->priority_message->state (type: unsigned int) */
	/* ( LPS[current_lp]->priority_message->state != NEW_EVT ) ? */
	movq		16(%rax), %rcx
	movq		(%rbx, %rcx, 1), %rcx
	testq		%rcx, %rcx
	jnz			.L1

	countoccurrences trampoline_count5

	/* %rcx <--- (&rt_data)->in_lpstate_bound_offset (type: size_t) */
	/* %rcx <--- LPS[current_lp]->bound (type: msg_t *) */
	/* ( LPS[current_lp]->bound == NULL ) ? */
	movq		8(%rax), %rcx
	movq		(%rdx, %rcx, 1), %rcx
	testq		%rcx, %rcx
	jz			.L1

	countoccurrences trampoline_count6

	/* %rdx <--- (&rt_data)->in_msg_timestamp_offset (type: size_t) */
	/* %xmm1 <--- LPS[current_lp]->bound->timestamp (type: double) */
	/* %xmm0 <--- LPS[current_lp]->priority_message->timestamp (type: double) */
	/* (  LPS[current_lp]->priority_message->timestamp >= LPS[current_lp]->bound->timestamp ) ? */
	movq		32(%rax), %rdx
	movsd		(%rcx, %rdx, 1), %xmm1
	movsd		(%rbx, %rdx, 1), %xmm0
	comisd		%xmm1, %xmm0
	jae			.L1

	countoccurrences trampoline_count7

/*      PREAMBLE END      */
/**************************/

	/* %rsi <--- 1 */
	movq		$0x1, %rsi

	/* %rdi <--- context_buffer */
	movq		%fs:0, %rdi
	addq		cntx_loop@gottpoff(%rip), %rdi

	/* RESTORES CURRENT CONTEXT */
	call		_long_jmp

/**************************/
/*    POSTAMBLE START     */

.L1:
	/* restore XMM0, XMM1 */
	movdqu		(%rsp), %xmm1
	movdqu		16(%rsp), %xmm0
	addq		$0x20, %rsp

	/* restore RCX, RDX */
	popq		%rdx
	popq		%rcx

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

	/* restore RBX, RAX */
	popq		%rbx
	popq		%rax

/*     POSTAMBLE END      */
/**************************/

	retq

.size cfv_trampoline, .-cfv_trampoline