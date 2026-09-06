//===- Casm.cpp -----------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Casm.h"
#include "Config.h"
#include "InputFiles.h"
#include "lld/Common/ErrorHandler.h"
#include "llvm/Support/MemoryBuffer.h"

#if LLD_CASM
#include "casm/Format.h"
#include "casm/Layout.h"
#include "casm/Model.h"
#include "casm/Serialize.h"
#endif

using namespace llvm;
using namespace lld;
using namespace lld::elf;

#if LLD_CASM

struct CasmFile::Impl {
  Impl(const casm::EncodeOptions &opts) : session(module, opts) {}
  casm::Module module;
  casm::LayoutSession session;
  casm::Object object;
};

#else

struct CasmFile::Impl {};

#endif

CasmFile::CasmFile() = default;
CasmFile::~CasmFile() = default;

#if LLD_CASM

bool elf::isCasm(MemoryBufferRef mb) {
  StringRef b = mb.getBuffer();
  return b.size() >= sizeof(casm::Magic) &&
         b.substr(0, sizeof(casm::Magic)) ==
             StringRef(casm::Magic, sizeof(casm::Magic));
}

CasmFile *elf::lowerCasm(Ctx &ctx, MemoryBufferRef mb) {
  casm::EncodeOptions opts;
  // An executable, and a shared object under -Bsymbolic, bind every defined
  // symbol to its definition.
  opts.FoldGlobals = !ctx.arg.shared || ctx.arg.bsymbolic == BsymbolicKind::All;
  auto cf = std::make_unique<CasmFile>();
  cf->impl = std::make_unique<CasmFile::Impl>(opts);
  CasmFile::Impl &impl = *cf->impl;
  std::string err;
  StringRef b = mb.getBuffer();
  if (!casm::read(reinterpret_cast<const uint8_t *>(b.data()), b.size(),
                  impl.module, err))
    Fatal(ctx) << mb.getBufferIdentifier() << ": " << err;
  std::vector<uint8_t> bytes;
  if (!impl.session.lower(casm::Placement{}, impl.object, err) ||
      !casm::writeELF(impl.object, bytes, err))
    Fatal(ctx) << mb.getBufferIdentifier() << ": " << err;
  std::unique_ptr<MemoryBuffer> buf = MemoryBuffer::getMemBufferCopy(
      StringRef(reinterpret_cast<const char *>(bytes.data()), bytes.size()),
      mb.getBufferIdentifier());
  cf->elf = buf->getMemBufferRef();
  CasmFile *ret = cf.get();
  std::lock_guard<std::mutex> lock(ctx.casmMutex);
  ctx.memoryBuffers.push_back(std::move(buf));
  ctx.casmFiles.push_back(std::move(cf));
  return ret;
}

#else

bool elf::isCasm(MemoryBufferRef) { return false; }

CasmFile *elf::lowerCasm(Ctx &ctx, MemoryBufferRef mb) {
  Fatal(ctx) << mb.getBufferIdentifier()
             << ": this lld was built without Casm support";
  return nullptr;
}

#endif
