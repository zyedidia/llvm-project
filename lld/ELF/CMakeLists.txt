set(LLVM_TARGET_DEFINITIONS Options.td)
tablegen(LLVM Options.inc -gen-opt-parser-defs)
add_public_tablegen_target(ELFOptionsTableGen)

if(LLVM_ENABLE_ZLIB)
  set(imported_libs ZLIB::ZLIB)
endif()

if(LLVM_ENABLE_ZSTD)
  if(TARGET zstd::libzstd_shared AND NOT LLVM_USE_STATIC_ZSTD)
    set(zstd_target zstd::libzstd_shared)
  else()
    set(zstd_target zstd::libzstd_static)
  endif()
endif()

if(LLVM_ENABLE_ZSTD)
  list(APPEND imported_libs ${zstd_target})
endif()

add_lld_library(lldELF
  AArch64ErrataFix.cpp
  Arch/AArch64.cpp
  Arch/AMDGPU.cpp
  Arch/ARM.cpp
  Arch/AVR.cpp
  Arch/Hexagon.cpp
  Arch/LoongArch.cpp
  Arch/Mips.cpp
  Arch/MipsArchTree.cpp
  Arch/MSP430.cpp
  Arch/PPC.cpp
  Arch/PPC64.cpp
  Arch/RISCV.cpp
  Arch/SPARCV9.cpp
  Arch/SystemZ.cpp
  Arch/X86.cpp
  Arch/X86_64.cpp
  ARMErrataFix.cpp
  BPSectionOrderer.cpp
  CallGraphSort.cpp
  Casm.cpp
  DWARF.cpp
  Driver.cpp
  DriverUtils.cpp
  EhFrame.cpp
  ICF.cpp
  InputFiles.cpp
  InputSection.cpp
  LTO.cpp
  LinkerScript.cpp
  MapFile.cpp
  MarkLive.cpp
  OutputSections.cpp
  Relocations.cpp
  ScriptLexer.cpp
  ScriptParser.cpp
  SymbolTable.cpp
  Symbols.cpp
  SyntheticSections.cpp
  Target.cpp
  Thunks.cpp
  Writer.cpp

  LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  BinaryFormat
  BitWriter
  Core
  DebugInfoDWARF
  Demangle
  DTLTO
  LTO
  MC
  Object
  Option
  Passes
  ProfileData
  Support
  TargetParser
  TransformUtils

  LINK_LIBS
  lldCommon
  ${imported_libs}
  ${LLVM_PTHREAD_LIB}

  DEPENDS
  ELFOptionsTableGen
  intrinsics_gen
  )

# libcasm, from the casm repository, lets lld read Casm modules. It has no
# LLVM dependency and is compiled from its sources into lldELF, so lld's
# export set is unchanged.
if(LLD_CASM_DIR)
  file(GLOB lld_casm_sources CONFIGURE_DEPENDS ${LLD_CASM_DIR}/lib/casm/*.cpp)
  target_sources(lldELF PRIVATE ${lld_casm_sources})
  target_include_directories(lldELF PRIVATE ${LLD_CASM_DIR}/include)
  target_compile_definitions(lldELF PRIVATE LLD_CASM=1)
  if(LLVM_ENABLE_ZSTD)
    set_source_files_properties(${lld_casm_sources} PROPERTIES
      COMPILE_DEFINITIONS CASM_HAVE_ZSTD=1)
  endif()
endif()
