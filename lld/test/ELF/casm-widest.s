# REQUIRES: x86, casm
## A module is lowered at its widest before the link places it: a jump the
## assembler left relaxable takes its rel32 form even to a target beside
## it, where the same object through the casm encoder alone takes rel8.

# RUN: %casm-as %s -o %t.casm
# RUN: %casm-obj %t.casm -o %t.o
# RUN: ld.lld -static %t.casm -o %t.exe
# RUN: llvm-objdump -d %t.exe | FileCheck %s
# CHECK: <_start>:
# CHECK-NEXT: e9 {{.*}} jmp
# RUN: ld.lld -static %t.o -o %t.elf.exe
# RUN: llvm-objdump -d %t.elf.exe | FileCheck --check-prefix=NARROW %s
# NARROW: <_start>:
# NARROW-NEXT: eb {{.*}} jmp

	.text
	.globl	_start
_start:
	jmp	.Lexit
	nop
.Lexit:
	movl	$60, %eax
	xorl	%edi, %edi
	syscall
