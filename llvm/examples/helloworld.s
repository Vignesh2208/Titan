	.text
	.file	"helloworld.c"
	.globl	main                    # -- Begin function main
	.p2align	4, 0x90
	.type	main,@function
main:                                   # @main
	.cfi_startproc
# %bb.0:
	pushfq
	pushq	%rax
	pushq	%rcx
	movq	currBurstLength@GOTPCREL(%rip), %rax
	movq	(%rax), %rcx
	subq	$12, %rcx
	movq	%rcx, (%rax)
	movq	currBBSize@GOTPCREL(%rip), %rcx
	movq	$12, (%rcx)
	movq	currBBID@GOTPCREL(%rip), %rcx
	movq	$3738921229792454695, (%rcx) # imm = 0x33E3510C80F22827
	movq	alwaysOn@GOTPCREL(%rip), %rcx
	callq	__Vt_Stub
	popq	%rcx
	popq	%rax
	popfq
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$16, %rsp
	movl	$0, -4(%rbp)
	movl	%edi, -8(%rbp)
	movq	%rsi, -16(%rbp)
	movb	$0, %al
	callq	Hello@PLT
	xorl	%eax, %eax
	addq	$16, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end0:
	.size	main, .Lfunc_end0-main
	.cfi_endproc
                                        # -- End function
	.section	.text.__Vt_Stub,"axG",@progbits,__Vt_Stub,comdat
	.weak	__Vt_Stub               # -- Begin function __Vt_Stub
	.p2align	4, 0x90
	.type	__Vt_Stub,@function
__Vt_Stub:                              # @__Vt_Stub
# %bb.4:
	cmpq	$0, (%rcx)
	jne	.LBB1_2
# %bb.3:
	cmpq	$0, (%rax)
	jg	.LBB1_0
.LBB1_2:
	pushq	%rdx
	pushq	%rsi
	pushq	%rdi
	pushq	%r8
	pushq	%r9
	pushq	%r10
	pushq	%r11
	callq	vtCallbackFn
	popq	%r11
	popq	%r10
	popq	%r9
	popq	%r8
	popq	%rdi
	popq	%rsi
	popq	%rdx
.LBB1_0:
	retq
.Lfunc_end1:
	.size	__Vt_Stub, .Lfunc_end1-__Vt_Stub
                                        # -- End function

	.ident	"clang version 9.0.1 "
	.section	".note.GNU-stack","",@progbits
	.addrsig
	.addrsig_sym Hello
