	.file	"x86_vt_spinner.c"
	.text
	.section	.rodata
.LC0:
	.string	"%Y-%m-%d %H:%M:%S"
.LC1:
	.string	"%s.%03ld\n"
	.text
	.globl	print_time
	.type	print_time, @function
print_time:
.LFB0:
	.cfi_startproc
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	subq	$80, %rsp
	movq	%fs:40, %rax
	movq	%rax, -8(%rbp)
	xorl	%eax, %eax
	leaq	-64(%rbp), %rax
	movl	$0, %esi
	movq	%rax, %rdi
	call	gettimeofday@PLT
	leaq	-64(%rbp), %rax
	movq	%rax, %rdi
	call	localtime@PLT
	movq	%rax, -80(%rbp)
	movq	-80(%rbp), %rdx
	leaq	-48(%rbp), %rax
	movq	%rdx, %rcx
	leaq	.LC0(%rip), %rdx
	movl	$40, %esi
	movq	%rax, %rdi
	call	strftime@PLT
	movq	-56(%rbp), %rcx
	movabsq	$2361183241434822607, %rdx
	movq	%rcx, %rax
	imulq	%rdx
	sarq	$7, %rdx
	movq	%rcx, %rax
	sarq	$63, %rax
	subq	%rax, %rdx
	movq	%rdx, %rax
	movq	%rax, -72(%rbp)
	movq	-72(%rbp), %rdx
	leaq	-48(%rbp), %rax
	movq	%rax, %rsi
	leaq	.LC1(%rip), %rdi
	movl	$0, %eax
	call	printf@PLT
	nop
	movq	-8(%rbp), %rax
	xorq	%fs:40, %rax
	je	.L2
	call	__stack_chk_fail@PLT
.L2:
	leave
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE0:
	.size	print_time, .-print_time
	.section	.rodata
.LC2:
	.string	"Interesting !"
	.text
	.globl	main
	.type	main, @function
main:
.LFB1:
	.cfi_startproc
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	subq	$32, %rsp
	movl	%edi, -20(%rbp)
	movq	%rsi, -32(%rbp)
	movl	$0, -8(%rbp)
	movl	$0, -4(%rbp)
	movl	interesting(%rip), %eax
	testl	%eax, %eax
	je	.L4
	leaq	.LC2(%rip), %rdi
	call	puts@PLT
.L4:
	movl	$0, -8(%rbp)
	jmp	.L5
.L8:
	movl	$0, -4(%rbp)
	jmp	.L6
.L7:
	addl	$1, -4(%rbp)
.L6:
	cmpl	$1023, -4(%rbp)
	jle	.L7
	addl	$1, -8(%rbp)
.L5:
	cmpl	$127, -8(%rbp)
	jle	.L8
.L9:
	jmp	.L9
	.cfi_endproc
.LFE1:
	.size	main, .-main
	.ident	"GCC: (Ubuntu 7.5.0-3ubuntu1~18.04) 7.5.0"
	.section	.note.GNU-stack,"",@progbits
