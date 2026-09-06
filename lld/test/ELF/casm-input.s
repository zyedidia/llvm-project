# REQUIRES: x86, casm
## A Casm module links like an ELF object: its sections, symbols and
## relocations reach the output as the same object assembled through the
## casm encoder would, and a module and an ELF object link together.

# RUN: rm -rf %t && split-file %s %t && cd %t
# RUN: %casm-as a.s -o a.casm
# RUN: %casm-as b.s -o b.casm
# RUN: %casm-obj a.casm -o a.o
# RUN: %casm-obj b.casm -o b.o

# RUN: ld.lld -static a.casm b.casm -o casm.exe
# RUN: ld.lld -static a.o b.o -o elf.exe
# RUN: llvm-objdump -d --no-show-raw-insn casm.exe | tail -n +3 > casm.dis
# RUN: llvm-objdump -d --no-show-raw-insn elf.exe | tail -n +3 > elf.dis
# RUN: diff casm.dis elf.dis
# RUN: llvm-readelf -s casm.exe | FileCheck %s
# CHECK-DAG: FUNC GLOBAL DEFAULT {{[0-9]+}} _start
# CHECK-DAG: FUNC GLOBAL DEFAULT {{[0-9]+}} ping
# CHECK-DAG: FUNC GLOBAL DEFAULT {{[0-9]+}} pong

## Mixed inputs.
# RUN: ld.lld -static a.casm b.o -o mixed.exe
# RUN: llvm-objdump -d --no-show-raw-insn mixed.exe | tail -n +3 > mixed.dis
# RUN: diff casm.dis mixed.dis

## A relocatable link keeps the module's relocations, each naming its
## symbol, as every lowering for a link does.
# RUN: ld.lld -r a.casm b.casm -o r.o
# RUN: llvm-readelf -r r.o | FileCheck --check-prefix=RELOC %s
# RELOC: R_X86_64_PLT32 {{.*}} pong
# RELOC: R_X86_64_PC32 {{.*}} msg

#--- a.s
	.text
	.globl	_start
	.type	_start, @function
_start:
	call	pong
	movl	%eax, %edx
	leaq	msg(%rip), %rsi
	movl	$1, %edi
	movl	$1, %eax
	syscall
	movl	$60, %eax
	xorl	%edi, %edi
	syscall

	.globl	ping
	.type	ping, @function
ping:
	movl	$8, %eax
	ret

	.section	.rodata
msg:
	.ascii	"lld ok\n"

#--- b.s
	.text
	.globl	pong
	.type	pong, @function
pong:
	call	ping
	ret
