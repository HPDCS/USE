.file	"trampoline.S"
.text

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

/*      PREAMBLE END      */
/**************************/

	/* save RSI, RDI */
	pushq		%rsi
	pushq		%rdi

	/* %rsi <--- 1 */
	movq		$0x1, %rsi

	/* %rdi <--- context_buffer */
	movq		%fs:0, %rdi
	addq		cntx_loop@gottpoff(%rip), %rdi

	/* RESTORES CURRENT CONTEXT */
	call		_long_jmp

	/*****************************************
	 * CURRENTLY THIS CONTEXT NEVER RESUMES. *
	 * IT MEANS THAT THE INSRUCTIONS BELOW   *
	 * NEVER EXECUTE. HOWEVER, EVERYTHING IS *
	 * SET FOR THE FUTURE!                   *
	 *****************************************/

	/* restore RSI, RDI */
	popq		%rdi
	popq		%rsi

/**************************/
/*    POSTAMBLE START     */

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

	/* restore RBX,RAX */
	popq		%rbx
	popq		%rax

	retq

/*     POSTAMBLE END      */
/**************************/

.size cfv_trampoline, .-cfv_trampoline