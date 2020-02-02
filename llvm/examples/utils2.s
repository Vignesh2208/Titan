	.text
	.file	"utils2.c"
	.section	.rodata.cst8,"aM",@progbits,8
	.p2align	3               # -- Begin function test2
.LCPI0_0:
	.quad	4607182418800017408     # double 1
	.text
	.globl	test2
	.p2align	4, 0x90
	.type	test2,@function
test2:                                  # @test2
	.cfi_startproc
# %bb.0:
	pushfq
	pushq	%rax
	pushq	%rcx
	movq	global_var_1@GOTPCREL(%rip), %rax
	movq	(%rax), %rcx
	subq	$19, %rcx
	movq	%rcx, (%rax)
	movq	global_var_2@GOTPCREL(%rip), %rcx
	movq	$100, (%rcx)
	movq	enabled@GOTPCREL(%rip), %rcx
	callq	__X86_Stub
	popq	%rcx
	popq	%rax
	popfq
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$32, %rsp
	movsd	.LCPI0_0(%rip), %xmm2   # xmm2 = mem[0],zero
	movsd	%xmm0, -8(%rbp)
	movsd	%xmm1, -16(%rbp)
	movsd	-8(%rbp), %xmm0         # xmm0 = mem[0],zero
	movsd	-16(%rbp), %xmm1        # xmm1 = mem[0],zero
	mulsd	-16(%rbp), %xmm1
	addsd	%xmm1, %xmm0
	movsd	%xmm2, -32(%rbp)        # 8-byte Spill
	callq	sin@PLT
	movsd	-32(%rbp), %xmm1        # 8-byte Reload
                                        # xmm1 = mem[0],zero
	addsd	%xmm0, %xmm1
	movsd	%xmm1, -24(%rbp)
	movsd	-24(%rbp), %xmm0        # xmm0 = mem[0],zero
	addq	$32, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end0:
	.size	test2, .Lfunc_end0-test2
	.cfi_endproc
                                        # -- End function
	.globl	Hello2                  # -- Begin function Hello2
	.p2align	4, 0x90
	.type	Hello2,@function
Hello2:                                 # @Hello2
	.cfi_startproc
# %bb.0:
	pushfq
	pushq	%rax
	pushq	%rcx
	movq	global_var_1@GOTPCREL(%rip), %rax
	movq	(%rax), %rcx
	subq	$9, %rcx
	movq	%rcx, (%rax)
	movq	global_var_2@GOTPCREL(%rip), %rcx
	movq	$100, (%rcx)
	movq	enabled@GOTPCREL(%rip), %rcx
	callq	__X86_Stub
	popq	%rcx
	popq	%rax
	popfq
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	movq	global_var_3@GOTPCREL(%rip), %rax
	movq	(%rax), %rsi
	leaq	.L.str(%rip), %rdi
	movb	$0, %al
	callq	printf@PLT
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end1:
	.size	Hello2, .Lfunc_end1-Hello2
	.cfi_endproc
                                        # -- End function
	.type	.L.str,@object          # @.str
	.section	.rodata.str1.1,"aMS",@progbits,1
.L.str:
	.asciz	"Hello World %llu\n"
	.size	.L.str, 18


	.ident	"clang version 9.0.1 "
	.section	".note.GNU-stack","",@progbits
	.addrsig
	.addrsig_sym sin
	.addrsig_sym printf
	.addrsig_sym global_var_3
