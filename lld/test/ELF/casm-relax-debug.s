# REQUIRES: x86, casm
## The debug sections lld relocates straight from the first lowering follow
## the code as it shrinks. After the jump takes rel8, the .debug_frame FDE
## and the line table still start at the function, since each names a
## symbol at the code rather than the section with an offset, and the line
## program keeps its offsets so its relocation lands on the address field.

# RUN: %casm-as %s -o %t.casm
# RUN: %casm-relax --grow %t.casm -o %t.grown.casm
# RUN: ld.lld -static --section-start=.text=0x201000 %t.grown.casm -o %t.exe
# RUN: llvm-dwarfdump --debug-frame %t.exe | FileCheck --check-prefix=FRAME %s
# FRAME: FDE {{.*}} pc=00201003...0020100c
# RUN: llvm-dwarfdump --debug-line %t.exe | FileCheck --check-prefix=LINE %s
# LINE:      0x0000000000201003 3 0 0 0 0 0 is_stmt
# LINE-NEXT: 0x0000000000201008 4 0 0 0 0 0 is_stmt
# LINE-NEXT: 0x000000000020100a 5 0 0 0 0 0 is_stmt
# LINE-NEXT: 0x000000000020100c 5 0 0 0 0 0 is_stmt end_sequence

	.text
	.globl	_start
_start:
	jmp	.Lexit
	nop
.Lexit:
	.cfi_sections .debug_frame
	.globl	f
	.type	f, @function
f:
	.cfi_startproc
	.file	0 "/src" "a.c"
	.loc	0 3 0
	movl	$60, %eax
	.loc	0 4 0
	xorl	%edi, %edi
	.loc	0 5 0
	syscall
	.cfi_endproc
	.size	f, .-f
