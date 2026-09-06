# REQUIRES: x86, casm
## The linker sizes every Casm relaxation site by the addresses it assigns:
## a call whose target is near takes its small form, one whose target the
## link places far takes the wide form, and the choice reaches the report.

# RUN: rm -rf %t && split-file %s %t && cd %t
# RUN: %casm-as a.s -o a.casm
# RUN: %casm-relax --grow a.casm -o grown.casm

## Adjacent: every site is small, and the program runs.
# RUN: ld.lld -static --section-start=.text=0x201000 --section-start=.fartext=0x202000 grown.casm -o near.exe --casm-relax-report=near.report
# RUN: FileCheck --check-prefix=NEAR %s < near.report
# NEAR-NOT: candidate {{[1-9]}}/
# NEAR: far candidate 0/2 size 5
# NEAR-NOT: candidate {{[1-9]}}/
# RUN: llvm-objdump -d near.exe | FileCheck --check-prefix=NEARDIS %s
# NEARDIS: <_start>:
# NEARDIS: callq {{.*}} <far>

## The far section 3GiB away: its call takes the wide movabs form.
# RUN: ld.lld -static --section-start=.text=0x201000 --section-start=.fartext=0xc0400000 grown.casm -o far.exe --casm-relax-report=far.report
# RUN: FileCheck --check-prefix=FAR %s < far.report
# FAR: far candidate 1/2 size 13
# RUN: llvm-objdump -d far.exe | FileCheck --check-prefix=FARDIS %s
# FARDIS: <_start>:
# FARDIS: movabsq $0xc0400000, %r11
# FARDIS-NEXT: callq *%r11

## Position-independent: the wide form's absolute field becomes a dynamic
## relocation, which the writer emits where the scan saw it, so the site
## cannot shrink away from under it.
# RUN: not ld.lld -pie -z notext grown.casm -o /dev/null 2>&1 | FileCheck --check-prefix=DYN %s
# DYN: error: grown.casm:(.text): a Casm candidate moved the field at offset 0x2, which lld made a dynamic relocation for

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
