# REQUIRES: x86, casm
## Relaxing a function that carries an FDE keeps unwind info in step. After
## `first` shrinks, its FDE and `second`'s, which follows it, cover the
## functions at their new addresses. The FDE of a function GC discards
## goes first, since the scan drops its relocations from the list the
## writer applies while the .eh_frame_hdr's list keeps them.

# RUN: %casm-as %s -o %t.casm
# RUN: %casm-relax --grow %t.casm -o %t.grown.casm
# RUN: ld.lld -static --eh-frame-hdr %t.grown.casm -o %t.exe
# RUN: llvm-dwarfdump --eh-frame %t.exe | grep -c FDE | FileCheck %s
# CHECK: 3

## _start is 14 bytes, first shrinks to three rel32 calls and a ret.
# RUN: ld.lld -static --gc-sections --eh-frame-hdr --section-start=.text=0x201000 %t.grown.casm -o %t.gc.exe
# RUN: llvm-dwarfdump --eh-frame %t.gc.exe | FileCheck --check-prefix=GC %s
# GC:      FDE {{.*}} pc=0020100e...0020101e
# GC-NEXT: Format:
# GC:      FDE {{.*}} pc=0020101e...0020101f
# GC-NOT:  FDE

	.section	.text.dead,"ax",@progbits
	.globl	dead
	.type	dead, @function
dead:
	.cfi_startproc
	ret
	.cfi_endproc
	.size	dead, .-dead

	.text
	.globl	_start
_start:
	call	first
	movl	$60, %eax
	xorl	%edi, %edi
	syscall

	.globl	first
	.type	first, @function
first:
	.cfi_startproc
	call	second
	call	second
	call	second
	.cfi_def_cfa_offset 16
	ret
	.cfi_endproc
	.size	first, .-first

	.globl	second
	.type	second, @function
second:
	.cfi_startproc
	ret
	.cfi_endproc
	.size	second, .-second
