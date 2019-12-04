.file	"trampoline.S"
.text

.align 4
.globl cfv_trampoline
.type cfv_trampoline, @function
cfv_trampoline:

	/* save RAX */
	pushq		%rax

	/* save FLAGS */
	lahf
	seto		%al
	pushq		%rax

	/* save RDI */
	pushq		%rdi

	/* %rdi <--- context_buffer */
	movq		%fs:0, %rdi
	addq		cntx@gottpoff(%rip), %rdi

	/* SAVES CURRENT CONTEXT */
	call		_set_jmp

	/* retval_of(_set_jmp) != 0 ? */
	testl		%eax, %eax
	jnz			.L1

	/*************** TODO *****************
	*                                     *
	*  call    use_check_lp_is_not_valid  *
	*                                     *
	**************************************/

	/* %rsi <--- 1 */
	movq		$0x1, %rsi

	/* %rdi <--- context_buffer */
	movq		%fs:0, %rdi
	addq		cntx@gottpoff(%rip), %rdi

	/* RESTORES CURRENT CONTEXT */
	call		_long_jmp

.L1:
	/* restore RDI */
	popq		%rdi

	/* restore FLAGS */
	popq		%rax
	addb		$0x7f, %al
	sahf

	/* restore RAX */
	popq		%rax

	retq

.size cfv_trampoline, .-cfv_trampoline