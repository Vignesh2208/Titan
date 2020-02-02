	.text
	.file	"utils.c"
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
	pushq	%rcx
	movq	currBurstLength@GOTPCREL(%rip), %rax
	movq	(%rax), %rcx
	subq	$19, %rcx
	movq	%rcx, (%rax)
	movq	currBBSize@GOTPCREL(%rip), %rcx
	movq	$19, (%rcx)
	movq	currBBID@GOTPCREL(%rip), %rcx
	movq	$5954110911255134470, (%rcx) # imm = 0x52A14053DAA3C106
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
	pushq	%rcx
	movq	currBurstLength@GOTPCREL(%rip), %rax
	movq	(%rax), %rcx
	subq	$4, %rcx
	movq	%rcx, (%rax)
	movq	currBBSize@GOTPCREL(%rip), %rcx
	movq	$4, (%rcx)
	movq	currBBID@GOTPCREL(%rip), %rcx
	movq	$1225826834863292516, (%rcx) # imm = 0x11030302F82B0064
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
.LBB1_1:                                # =>This Inner Loop Header: Depth=1
	pushfq
	pushq	%rax
	pushq	%rcx
	movq	currBurstLength@GOTPCREL(%rip), %rax
	movq	(%rax), %rcx
	subq	$2, %rcx
	movq	%rcx, (%rax)
	movq	currBBSize@GOTPCREL(%rip), %rcx
	movq	$2, (%rcx)
	movq	currBBID@GOTPCREL(%rip), %rcx
	movq	$3068467853311322268, (%rcx) # imm = 0x2A9563415153A89C
	movq	alwaysOn@GOTPCREL(%rip), %rcx
	callq	__Vt_Stub
	popq	%rcx
	popq	%rax
	popfq
	cmpl	$5, -4(%rbp)
	jge	.LBB1_4
# %bb.2:                                #   in Loop: Header=BB1_1 Depth=1
	pushfq
	pushq	%rax
	pushq	%rcx
	movq	currBurstLength@GOTPCREL(%rip), %rax
	movq	(%rax), %rcx
	subq	$4, %rcx
	movq	%rcx, (%rax)
	movq	currBBSize@GOTPCREL(%rip), %rcx
	movq	$4, (%rcx)
	movq	currBBID@GOTPCREL(%rip), %rcx
	movq	$158232574614313924, (%rcx) # imm = 0x23227B031C81BC4
	movq	alwaysOn@GOTPCREL(%rip), %rcx
	callq	__Vt_Stub
	popq	%rcx
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
	pushq	%rcx
	movq	currBurstLength@GOTPCREL(%rip), %rax
	movq	(%rax), %rcx
	subq	$4, %rcx
	movq	%rcx, (%rax)
	movq	currBBSize@GOTPCREL(%rip), %rcx
	movq	$4, (%rcx)
	movq	currBBID@GOTPCREL(%rip), %rcx
	movq	$1910821065714192530, (%rcx) # imm = 0x1A8499A2A33A5092
	movq	alwaysOn@GOTPCREL(%rip), %rcx
	callq	__Vt_Stub
	popq	%rcx
	popq	%rax
	popfq
	movl	-4(%rbp), %eax
	addl	$1, %eax
	movl	%eax, -4(%rbp)
	jmp	.LBB1_1
.LBB1_4:
	pushfq
	pushq	%rax
	pushq	%rcx
	movq	currBurstLength@GOTPCREL(%rip), %rax
	movq	(%rax), %rcx
	subq	$8, %rcx
	movq	%rcx, (%rax)
	movq	currBBSize@GOTPCREL(%rip), %rcx
	movq	$8, (%rcx)
	movq	currBBID@GOTPCREL(%rip), %rcx
	movq	$5595011296698436052, (%rcx) # imm = 0x4DA57921760209D4
	movq	alwaysOn@GOTPCREL(%rip), %rcx
	callq	__Vt_Stub
	popq	%rcx
	popq	%rax
	popfq
	movq	currBurstLength@GOTPCREL(%rip), %rax
	movq	(%rax), %rsi
	leaq	.L.str(%rip), %rdi
	movb	$0, %al
	callq	printf@PLT
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
	.addrsig_sym currBurstLength
