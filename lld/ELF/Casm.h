//===- Casm.h -------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Casm modules as inputs. A module is an instrumentable object whose
// encodings are decided at layout, so it is lowered to an ELF object for the
// ordinary input path and lowered again as the link places it.
//
//===----------------------------------------------------------------------===//

#ifndef LLD_ELF_CASM_H
#define LLD_ELF_CASM_H

#include "llvm/Support/MemoryBufferRef.h"

#include <memory>

namespace lld::elf {
struct Ctx;
class ELFFileBase;

// A module lowered for the link: the module, the session that lowers it and
// the object of its last lowering behind impl, the ELF bytes the input path
// parsed, and the file built from them.
struct CasmFile {
  CasmFile();
  ~CasmFile();
  struct Impl;
  std::unique_ptr<Impl> impl;
  llvm::MemoryBufferRef elf;
  ELFFileBase *file = nullptr;
};

// Whether the buffer holds a Casm module.
bool isCasm(llvm::MemoryBufferRef mb);

// Lower the module in mb at its widest and register it with ctx. The
// result's elf buffer is what the input path parses. Fatal on a module lld
// cannot read or lower. Safe to call from several threads.
CasmFile *lowerCasm(Ctx &ctx, llvm::MemoryBufferRef mb);

} // namespace lld::elf

#endif
