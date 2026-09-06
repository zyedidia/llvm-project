# REQUIRES: x86, casm
## An archive of Casm modules: a member is extracted for a symbol the link
## needs and left out otherwise, and --whole-archive takes every member.

# RUN: rm -rf %t && split-file %s %t && cd %t
# RUN: %casm-as main.s -o main.casm
# RUN: %casm-as used.s -o used.casm
# RUN: %casm-as unused.s -o unused.casm
# RUN: rm -f lib.a && llvm-ar rcs lib.a used.casm unused.casm

# RUN: ld.lld -static main.casm lib.a -o lazy.exe
# RUN: llvm-readelf -s lazy.exe | FileCheck --check-prefix=LAZY --implicit-check-not=unused %s
# LAZY: FUNC GLOBAL DEFAULT {{[0-9]+}} used{{$}}
# LAZY-NOT: unused
# RUN: ld.lld -static main.casm --whole-archive lib.a -o whole.exe
# RUN: llvm-readelf -s whole.exe | FileCheck --check-prefix=WHOLE %s
# WHOLE-DAG: FUNC GLOBAL DEFAULT {{[0-9]+}} used{{$}}
# WHOLE-DAG: FUNC GLOBAL DEFAULT {{[0-9]+}} unused{{$}}

#--- main.s
	.text
	.globl	_start
_start:
	call	used
	movl	$60, %eax
	xorl	%edi, %edi
	syscall

#--- used.s
	.text
	.globl	used
	.type	used, @function
used:
	ret

#--- unused.s
	.text
	.globl	unused
	.type	unused, @function
unused:
	ret
