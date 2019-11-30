.file	"trampoline.S"
		.text

.globl	cfv_trampoline
		.type	cfv_trampoline, @function

cfv_trampoline:

	# save RAX
	pushq		%rax

	# save FLAGS
	lahf
	seto		%al
	pushq		%rax

	# TODO: additional filters should be placed here;
	#       before giving control to the USE function
	#       in charge of performing rollbacks

	# restore FLAGS
	popq		%rax
	addb		$0x7f, %al
	sahf

	# restore RAX
	popq		%rax

	retq

.size	cfv_trampoline, .-cfv_trampoline