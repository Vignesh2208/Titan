	.text
	.file	"utils1.c"
	.section	.rodata.cst8,"aM",@progbits,8
	.p2align	3               # -- Begin function test
.LCPI0_0:
	.quad	4607182418800017408     # double 1
	.text
	.globl	test
	.p2align	4, 0x90
	.type	test,@function
test:                                   # @test
	.cfi_startproc
# %bb.0:
	pushfq
	pushq	%rax
	movq	global_var_1@GOTPCREL(%rip), %rax
	subq	$19, %rax
	movq	%rax, global_var_1@GOTPCREL(%rip)
	movq	$100, global_var_2@GOTPCREL(%rip)
	callq	__X86_Stub
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
	.size	test, .Lfunc_end0-test
	.cfi_endproc
                                        # -- End function
	.globl	Hello                   # -- Begin function Hello
	.p2align	4, 0x90
	.type	Hello,@function
Hello:                                  # @Hello
	.cfi_startproc
# %bb.0:
	pushfq
	pushq	%rax
	movq	global_var_1@GOTPCREL(%rip), %rax
	subq	$4, %rax
	movq	%rax, global_var_1@GOTPCREL(%rip)
	movq	$100, global_var_2@GOTPCREL(%rip)
	callq	__X86_Stub
	popq	%rax
	popfq
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$16, %rsp
	movl	$0, -4(%rbp)
.LBB1_1:                                # =>This Inner Loop Header: Depth=1
	pushfq
	pushq	%rax
	movq	global_var_1@GOTPCREL(%rip), %rax
	subq	$2, %rax
	movq	%rax, global_var_1@GOTPCREL(%rip)
	movq	$100, global_var_2@GOTPCREL(%rip)
	callq	__X86_Stub
	popq	%rax
	popfq
	cmpl	$5, -4(%rbp)
	jge	.LBB1_4
# %bb.2:                                #   in Loop: Header=BB1_1 Depth=1
	pushfq
	pushq	%rax
	movq	global_var_1@GOTPCREL(%rip), %rax
	subq	$4, %rax
	movq	%rax, global_var_1@GOTPCREL(%rip)
	movq	$100, global_var_2@GOTPCREL(%rip)
	callq	__X86_Stub
	popq	%rax
	popfq
	xorps	%xmm0, %xmm0
	movsd	%xmm0, -16(%rbp)        # 8-byte Spill
	movsd	-16(%rbp), %xmm1        # 8-byte Reload
                                        # xmm1 = mem[0],zero
	callq	test@PLT
# %bb.3:                                #   in Loop: Header=BB1_1 Depth=1
	pushfq
	pushq	%rax
	movq	global_var_1@GOTPCREL(%rip), %rax
	subq	$4, %rax
	movq	%rax, global_var_1@GOTPCREL(%rip)
	movq	$100, global_var_2@GOTPCREL(%rip)
	callq	__X86_Stub
	popq	%rax
	popfq
	movl	-4(%rbp), %eax
	addl	$1, %eax
	movl	%eax, -4(%rbp)
	jmp	.LBB1_1
.LBB1_4:
	pushfq
	pushq	%rax
	movq	global_var_1@GOTPCREL(%rip), %rax
	subq	$8, %rax
	movq	%rax, global_var_1@GOTPCREL(%rip)
	movq	$100, global_var_2@GOTPCREL(%rip)
	callq	__X86_Stub
	popq	%rax
	popfq
	movq	enabled_2@GOTPCREL(%rip), %rax
	movq	global_var_3@GOTPCREL(%rip), %rcx
	movq	$100, (%rcx)
	movq	(%rcx), %rdx
	subq	$5, %rdx
	movq	%rdx, (%rcx)
	cmpl	$0, (%rax)
	jne	.LBB1_6
# %bb.5:
	pushfq
	pushq	%rax
	movq	global_var_1@GOTPCREL(%rip), %rax
	subq	$3, %rax
	movq	%rax, global_var_1@GOTPCREL(%rip)
	movq	$100, global_var_2@GOTPCREL(%rip)
	callq	__X86_Stub
	popq	%rax
	popfq
	movq	global_var_3@GOTPCREL(%rip), %rax
	cmpq	$0, (%rax)
	jge	.LBB1_7
.LBB1_6:
	pushfq
	pushq	%rax
	movq	global_var_1@GOTPCREL(%rip), %rax
	subq	$5, %rax
	movq	%rax, global_var_1@GOTPCREL(%rip)
	movq	$100, global_var_2@GOTPCREL(%rip)
	callq	__X86_Stub
	popq	%rax
	popfq
	movq	global_var_3@GOTPCREL(%rip), %rax
	movq	(%rax), %rsi
	leaq	.L.str(%rip), %rdi
	movb	$0, %al
	callq	printf@PLT
.LBB1_7:
	pushfq
	pushq	%rax
	movq	global_var_1@GOTPCREL(%rip), %rax
	subq	$3, %rax
	movq	%rax, global_var_1@GOTPCREL(%rip)
	movq	$100, global_var_2@GOTPCREL(%rip)
	callq	__X86_Stub
	popq	%rax
	popfq
	addq	$16, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end1:
	.size	Hello, .Lfunc_end1-Hello
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
	.addrsig_sym test
	.addrsig_sym sin
	.addrsig_sym printf
	.addrsig_sym global_var_3
	.addrsig_sym enabled_2
