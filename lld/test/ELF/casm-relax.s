# REQUIRES: x86, casm
## The linker sizes every Casm relaxation site by the addresses it assigns:
## a call whose target is near takes its small form, one whose target the
## link places far takes a wide form, a chain of PC-relative leas, and the
## choice reaches the report. The chain carries no absolute relocation, so
## the same module links as a PIE.

# RUN: rm -rf %t && split-file %s %t && cd %t
# RUN: %casm-as a.s -o a.casm
# RUN: %casm-relax --grow a.casm -o grown.casm

## Adjacent: every site is small, and the program runs.
# RUN: ld.lld -static --section-start=.text=0x201000 --section-start=.fartext=0x202000 grown.casm -o near.exe --casm-relax-report=near.report
# RUN: FileCheck --check-prefix=NEAR %s < near.report
# NEAR-NOT: candidate {{[1-9]}}/
# NEAR: far candidate 0/7 size 5
# NEAR-NOT: candidate {{[1-9]}}/
# RUN: llvm-objdump -d near.exe | FileCheck --check-prefix=NEARDIS %s
# NEARDIS: <_start>:
# NEARDIS: callq {{.*}} <far>

## The far section 3GiB away: its call takes the shortest chain that
## reaches, whose relocated lea is followed by one adding 2GiB.
# RUN: ld.lld -static --section-start=.text=0x201000 --section-start=.fartext=0xc0400000 grown.casm -o far.exe --casm-relax-report=far.report
# RUN: FileCheck --check-prefix=FAR %s < far.report
# FAR: far candidate 1/7 size 17
# RUN: llvm-objdump -d far.exe | FileCheck --check-prefix=FARDIS %s
# FARDIS: <_start>:
# FARDIS-NEXT: leaq {{.*}}(%rip), %r11
# FARDIS-NEXT: leaq 0x7fffffff(%r11), %r11
# FARDIS-NEXT: callq *%r11

## Position-independent: the chain is PC-relative throughout, so the same
## layout links as a PIE and runs through it.
# RUN: ld.lld -pie --section-start=.text=0x201000 --section-start=.fartext=0xc0400000 grown.casm -o pie.exe --casm-relax-report=pie.report
# RUN: FileCheck --check-prefix=FAR %s < pie.report
# RUN: ./pie.exe

#--- a.s
	.text
	.globl	_start
	.type	_start, @function
_start:
	call	far
	movl	$60, %eax
	xorl	%edi, %edi
	syscall
	.size	_start, .-_start

	.section	.fartext, "ax", @progbits
	.globl	far
	.type	far, @function
far:
	ret
	.size	far, .-far
