# REQUIRES: x86, casm
## A label after a site that shrinks moves, and every reference to it
## follows: one from a section lld relocates straight from the lowered
## object, as it does every non-alloc section, and one lld turns into a
## dynamic relocation whose addend it fixes at the scan. Both read the
## symbol's value, which relaxation keeps current.

# RUN: rm -rf %t && split-file %s %t && cd %t
# RUN: %casm-as a.s -o a.casm
# RUN: %casm-relax --grow a.casm -o grown.casm

## The jump takes rel8 again, so .Lexit sits after two bytes and a nop.
# RUN: ld.lld -static --section-start=.text=0x201000 grown.casm -o exe
# RUN: llvm-objdump -s -j .debug_foo exe | FileCheck %s
# CHECK: 03102000 00000000

# RUN: ld.lld -pie --section-start=.text=0x201000 grown.casm -o pie
# RUN: llvm-readobj -r pie | FileCheck --check-prefix=PIE %s
# PIE: R_X86_64_RELATIVE - 0x201003

#--- a.s
	.text
	.globl	_start
_start:
	jmp	.Lexit
	nop
.Lexit:
	movl	$60, %eax
	xorl	%edi, %edi
	syscall

	.section	.debug_foo,"",@progbits
	.quad	.Lexit

	.data
	.quad	.Lexit
