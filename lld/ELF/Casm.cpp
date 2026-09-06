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
#include "InputSection.h"
#include "OutputSections.h"
#include "Symbols.h"
#include "SyntheticSections.h"
#include "Target.h"
#include "lld/Common/ErrorHandler.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/TimeProfiler.h"
#include "llvm/Support/raw_ostream.h"

#if LLD_CASM
#include "casm/Format.h"
#include "casm/Layout.h"
#include "casm/Model.h"
#include "casm/Serialize.h"
#endif

using namespace llvm;
using namespace llvm::ELF;
using namespace llvm::object;
using namespace lld;
using namespace lld::elf;

#if LLD_CASM

struct CasmFile::Impl {
  Impl(const casm::EncodeOptions &opts) : session(module, opts) {}
  casm::Module module;
  casm::LayoutSession session;
  casm::Object object;
  // The encoder's account of the last lowering's sites.
  std::string report;

  // Relaxation state. The content of each section as last lowered, which
  // the file's sections point into. For each relocation list lld keeps of
  // a section, the object relocation each entry came from with the type
  // and expression the scan gave it, which it takes again whenever a site
  // returns to its widest form. And the object relocations the scan turned
  // into dynamic relocations, which the writer emits where the scan saw
  // them, so they must hold their offset and type.
  struct RelocMap {
    uint32_t objIdx;
    RelType origType;
    RelExpr origExpr;
  };
  bool prepared = false;
  std::vector<std::vector<uint8_t>> contents;
  std::vector<SmallVector<std::vector<RelocMap>, 2>> relocMaps;
  std::vector<std::vector<uint32_t>> dynRelocs;
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
  auto cf = std::make_unique<CasmFile>();
  cf->impl = std::make_unique<CasmFile::Impl>(opts);
  CasmFile::Impl &impl = *cf->impl;
  impl.session.report(&impl.report);
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

namespace {

// The expression a candidate's relocation takes, for the types a
// candidate may switch to: the absolute and PC-relative fields, with a
// PLT reference to a symbol the link binds directly a plain PC-relative
// one, as the scan decides. Any other type keeps what the scan gave it at
// the widest lowering.
bool exprFor(RelType type, const Symbol &sym, RelExpr &expr) {
  switch (type) {
  case R_X86_64_8:
  case R_X86_64_16:
  case R_X86_64_32:
  case R_X86_64_32S:
  case R_X86_64_64:
    expr = R_ABS;
    return true;
  case R_X86_64_PC8:
  case R_X86_64_PC16:
  case R_X86_64_PC32:
  case R_X86_64_PC64:
    expr = R_PC;
    return true;
  case R_X86_64_PLT32:
    expr = sym.isPreemptible || sym.isGnuIFunc() ? R_PLT_PC : R_PC;
    return true;
  default:
    return false;
  }
}

// Whether sec is one of file's own input sections.
bool ownSection(const SectionBase *sec, const InputFile *file) {
  auto *isec = dyn_cast_or_null<InputSectionBase>(sec);
  return isec && isec->file == file;
}

// A relocation list a section keeps, derived from the object's relocations
// in order with some dropped: the scan skips a marker, the second half of
// a TLS sequence it relaxed, or an entry in a piece GC discarded. Shifted
// when its offsets are in the merged .eh_frame's coordinates.
struct RelocList {
  MutableArrayRef<Relocation> rels;
  bool shifted;
};

// Every list the section keeps. relocs() is what the writer applies; an
// EhInputSection also keeps rels, input-relative, which the .eh_frame_hdr
// reads by index.
SmallVector<RelocList, 2> relocLists(InputSectionBase &isec) {
  if (auto *eh = dyn_cast<EhInputSection>(&isec))
    return {{eh->rels, false}, {isec.relocs(), true}};
  return {{isec.relocs(), false}};
}

// The offset of an object relocation in a list's coordinates.
uint64_t listOffset(const InputSectionBase &isec, bool shifted,
                    uint64_t offset) {
  if (!shifted)
    return offset;
  return cast<EhInputSection>(isec).getParentOffset(offset);
}

// Pair each relocation of a list with the object relocation it came from,
// walking both in order.
bool mapRelocs(Ctx &ctx, CasmFile::Impl &impl, uint32_t idx,
               InputSectionBase &isec, const RelocList &list,
               std::vector<CasmFile::Impl::RelocMap> &map) {
  const std::vector<casm::ObjReloc> &objRels = impl.object.Sections[idx].Relocs;
  size_t j = 0;
  for (const Relocation &rel : list.rels) {
    while (j < objRels.size() &&
           (listOffset(isec, list.shifted, objRels[j].Offset) != rel.offset ||
            objRels[j].Type != rel.type))
      ++j;
    if (j == objRels.size()) {
      Err(ctx) << &isec << ": cannot pair a relocation at offset 0x"
               << Twine::utohexstr(rel.offset) << " with the module's";
      return false;
    }
    map.push_back({uint32_t(j), rel.type, rel.expr});
    ++j;
  }
  return true;
}

// For every section of a Casm file, the offsets its dynamic relocations
// cover.
using DynOffsets = DenseMap<const InputSectionBase *, DenseSet<uint64_t>>;

DynOffsets dynamicOffsets(Ctx &ctx) {
  DynOffsets offs;
  auto isCasmSec = [](const InputSectionBase *isec) {
    auto *f = dyn_cast_or_null<ELFFileBase>(isec ? isec->file : nullptr);
    return f && f->casm;
  };
  if (RelocationBaseSection *rela = ctx.in.relaDyn.get())
    for (const SmallVector<DynamicReloc, 0> *v :
         {&rela->relocs, &rela->relativeRelocs})
      for (const DynamicReloc &r : *v)
        if (isCasmSec(r.inputSec))
          offs[r.inputSec].insert(r.offsetInSec);
  for (RelrBaseSection *relr : {ctx.in.relrDyn.get(), ctx.in.relrAuthDyn.get()})
    if (relr)
      for (const RelativeReloc &r : relr->relocs)
        if (isCasmSec(r.inputSec))
          offs[r.inputSec].insert(r.inputSec->relocs()[r.relocIdx].offset);
  return offs;
}

// Build the file's relocation maps and note its dynamic relocations.
bool prepareFile(Ctx &ctx, CasmFile::Impl &impl,
                 function_ref<InputSectionBase *(size_t)> sectionOf,
                 const DynOffsets &dyn) {
  size_t ns = impl.object.Sections.size();
  impl.contents.resize(ns);
  impl.relocMaps.resize(ns);
  impl.dynRelocs.resize(ns);
  for (size_t i = 0; i < ns; ++i) {
    InputSectionBase *isec = sectionOf(i);
    if (!isec)
      continue;
    for (const RelocList &list : relocLists(*isec)) {
      impl.relocMaps[i].emplace_back();
      if (!mapRelocs(ctx, impl, i, *isec, list, impl.relocMaps[i].back()))
        return false;
    }
    auto it = dyn.find(isec);
    if (it == dyn.end())
      continue;
    bool shifted = isa<EhInputSection>(isec);
    const std::vector<casm::ObjReloc> &objRels = impl.object.Sections[i].Relocs;
    for (size_t j = 0; j < objRels.size(); ++j)
      if (it->second.contains(listOffset(*isec, shifted, objRels[j].Offset)))
        impl.dynRelocs[i].push_back(uint32_t(j));
  }
  impl.prepared = true;
  return true;
}

// Relax one file. Returns whether any of its sections changed size or any
// of its symbols moved, either of which another site may depend on.
bool relaxFile(Ctx &ctx, CasmFile &cf, const DynOffsets &dyn) {
  CasmFile::Impl &impl = *cf.impl;
  auto *file = cast<ObjFile<ELF64LE>>(cf.file);
  ArrayRef<InputSectionBase *> secs = file->getSections();
  size_t ns = impl.object.Sections.size();
  auto sectionOf = [&](size_t i) -> InputSectionBase * {
    InputSectionBase *isec = 1 + i < secs.size() ? secs[1 + i] : nullptr;
    if (!isec || isec == &InputSection::discarded)
      return nullptr;
    return isec;
  };

  if (!impl.prepared && !prepareFile(ctx, impl, sectionOf, dyn))
    return false;

  // What the link knows: where each section is, where each symbol is
  // that the link placed rather than the module, and which globals bind
  // to the module's own definition.
  casm::Placement p;
  p.SectionBases.assign(ns, std::nullopt);
  p.SymbolAddrs.assign(impl.object.Symbols.size(), std::nullopt);
  p.SymbolBinds.assign(impl.object.Symbols.size(), std::nullopt);
  for (size_t i = 0; i < ns; ++i) {
    InputSectionBase *isec = sectionOf(i);
    if (isec && (isec->flags & SHF_ALLOC) && isec->getOutputSection())
      p.SectionBases[i] = isec->getVA(0);
  }
  for (size_t i = 1; i < impl.object.Symbols.size(); ++i) {
    const casm::ObjSymbol &y = impl.object.Symbols[i];
    if (y.Type == STT_SECTION)
      continue;
    Symbol &s = file->getSymbol(i);
    auto *d = dyn_cast<Defined>(&s);
    // A definition in one of the module's own sections is where the
    // module puts it, unless the section is merged entry by entry.
    if (d && d->file == file && (!d->section || ownSection(d->section, file)) &&
        !isa_and_nonnull<MergeInputSection>(d->section)) {
      if (y.Binding != STB_LOCAL)
        p.SymbolBinds[i] = !s.isPreemptible;
      continue;
    }
    if (s.isPreemptible || s.isGnuIFunc())
      continue;
    if (d || s.isUndefined())
      p.SymbolAddrs[i] = s.getVA(ctx);
  }

  casm::Object next;
  std::string err;
  if (!impl.session.lower(p, next, err)) {
    Err(ctx) << cf.elf.getBufferIdentifier() << ": " << err;
    return false;
  }

  bool changed = false;
  for (size_t i = 0; i < ns; ++i) {
    InputSectionBase *isec = sectionOf(i);
    if (!isec)
      continue;
    casm::ObjSection &sec = next.Sections[i];
    for (uint32_t j : impl.dynRelocs[i]) {
      const casm::ObjReloc &was = impl.object.Sections[i].Relocs[j];
      if (sec.Relocs[j].Offset == was.Offset && sec.Relocs[j].Type == was.Type)
        continue;
      Err(ctx) << isec << ": a Casm candidate moved the field at offset 0x"
               << Twine::utohexstr(was.Offset)
               << ", which lld made a dynamic relocation for";
      return false;
    }
    if (isec->size != sec.Size)
      changed = true;
    impl.contents[i] = std::move(sec.Content);
    isec->content_ = impl.contents[i].data();
    isec->size = sec.Size;
    bool eh = isa<EhInputSection>(isec);
    SmallVector<RelocList, 2> lists = relocLists(*isec);
    for (size_t l = 0; l < lists.size(); ++l) {
      MutableArrayRef<Relocation> rels = lists[l].rels;
      const std::vector<CasmFile::Impl::RelocMap> &map = impl.relocMaps[i][l];
      for (size_t k = 0; k < rels.size(); ++k) {
        const CasmFile::Impl::RelocMap &m = map[k];
        const casm::ObjReloc &r = sec.Relocs[m.objIdx];
        Relocation &rel = rels[k];
        // A merged .eh_frame keeps a constant size, so its relocation
        // offsets never move; an ordinary section's do as it shrinks.
        if (!eh)
          rel.offset = r.Offset;
        rel.addend = r.Addend;
        rel.sym = &file->getSymbol(r.Sym);
        if (r.Type != rel.type) {
          rel.type = r.Type;
          if (r.Type == m.origType)
            rel.expr = m.origExpr;
          else if (!exprFor(r.Type, *rel.sym, rel.expr))
            Err(ctx) << isec << ": a Casm candidate uses relocation type "
                     << r.Type << ", which relaxation does not handle";
        }
      }
    }
  }
  for (size_t i = 1; i < next.Symbols.size(); ++i) {
    const casm::ObjSymbol &y = next.Symbols[i];
    if (y.Type == STT_SECTION || y.Place != casm::SymPlace::Section)
      continue;
    auto *d = dyn_cast<Defined>(&file->getSymbol(i));
    if (!d || d->file != file || !ownSection(d->section, file))
      continue;
    if (d->value != y.Value || d->size != y.Size)
      changed = true;
    d->value = y.Value;
    d->size = y.Size;
  }
  impl.object = std::move(next);
  return changed;
}

} // namespace

bool elf::relaxCasm(Ctx &ctx, int pass) {
  llvm::TimeTraceScope timeScope("Casm relaxation");
  // Every dynamic relocation exists before the first pass prepares.
  DynOffsets dyn;
  if (llvm::any_of(ctx.casmFiles,
                   [](auto &cf) { return cf->file && !cf->impl->prepared; }))
    dyn = dynamicOffsets(ctx);
  bool changed = false;
  for (std::unique_ptr<CasmFile> &cf : ctx.casmFiles)
    if (cf->file && !cf->file->lazy)
      changed |= relaxFile(ctx, *cf, dyn);
  return changed;
}

void elf::finalizeCasm(Ctx &ctx) {
  if (ctx.arg.casmRelaxReport.empty())
    return;
  std::error_code ec;
  raw_fd_ostream os(ctx.arg.casmRelaxReport, ec, sys::fs::OF_None);
  if (ec) {
    Err(ctx) << "cannot open " << ctx.arg.casmRelaxReport << ": "
             << ec.message();
    return;
  }
  for (std::unique_ptr<CasmFile> &cf : ctx.casmFiles)
    if (cf->file && !cf->file->lazy)
      os << cf->impl->report;
}

#else

bool elf::isCasm(MemoryBufferRef) { return false; }

CasmFile *elf::lowerCasm(Ctx &ctx, MemoryBufferRef mb) {
  Fatal(ctx) << mb.getBufferIdentifier()
             << ": this lld was built without Casm support";
  return nullptr;
}

bool elf::relaxCasm(Ctx &, int) { return false; }

void elf::finalizeCasm(Ctx &) {}

#endif
