//===- SafeHeap.cpp - Safe Heap Instrumentation ---------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/CodeGen/SafeHeap.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/InitializePasses.h"
#include "llvm/Transforms/Utils/Local.h"
#include <cassert>

using namespace llvm;

#define DEBUG_TYPE "safe-heap"

namespace {

class SafeHeap {
  Function &F;

public:
  SafeHeap(Function &F) : F(F) {}

  bool run();
};

class SafeHeapLegacyPass : public FunctionPass {
  const TargetMachine *TM = nullptr;

public:
  static char ID;

  SafeHeapLegacyPass() : FunctionPass(ID) {
    initializeSafeHeapLegacyPassPass(*PassRegistry::getPassRegistry());
  }

  bool runOnFunction(Function &F) override {
    return SafeHeap(F).run();
  }
};

} // end anonymous namespace

Value *readRegister(IRBuilder<> &IRB, StringRef Name) {
  Module *M = IRB.GetInsertBlock()->getParent()->getParent();
  MDNode *MD =
      MDNode::get(M->getContext(), {MDString::get(M->getContext(), Name)});
  Value *Args[] = {MetadataAsValue::get(M->getContext(), MD)};
  return IRB.CreateIntrinsic(Intrinsic::read_register,
                             IRB.getIntPtrTy(M->getDataLayout()), Args);
}

bool SafeHeap::run() {
  errs() << "[SafeHeap] run Function: " << F.getName() << "\n";

  SmallVector<Instruction*, 64> ToInstrument;

  // Collect loads and stores (we collect first to avoid iterator invalidation)
  for (Instruction &I : instructions(F))
    if (isa<LoadInst>(&I) || isa<StoreInst>(&I))
      ToInstrument.push_back(&I);

  for (Instruction *I : ToInstrument) {
    if (LoadInst *LI = dyn_cast<LoadInst>(I)) {
      IRBuilder<> B(LI);
      Value *Base = readRegister(B, "x27");
      Value *Addr64 = B.CreatePtrToInt(LI->getPointerOperand(), B.getInt64Ty());
      Value *Addr32 = B.CreateTrunc(Addr64, B.getInt32Ty());
      Value *Addr32Ext = B.CreateZExt(Addr32, B.getInt64Ty());
      Value *AddrMasked = B.CreateAdd(Base, Addr32Ext);
      Value *PtrMasked = B.CreateIntToPtr(AddrMasked, B.getPtrTy());
      LI->setOperand(0, PtrMasked);
    } else if (StoreInst *SI = dyn_cast<StoreInst>(I)) {
      IRBuilder<> B(SI);
      Value *Base = readRegister(B, "x27");
      Value *Addr64 = B.CreatePtrToInt(SI->getPointerOperand(), B.getInt64Ty());
      Value *Addr32 = B.CreateTrunc(Addr64, B.getInt32Ty());
      Value *Addr32Ext = B.CreateZExt(Addr32, B.getInt64Ty());
      Value *AddrMasked = B.CreateAdd(Base, Addr32Ext);
      Value *PtrMasked = B.CreateIntToPtr(AddrMasked, B.getPtrTy());
      SI->setOperand(1, PtrMasked);
    }
  }

  return false;
}

char SafeHeapLegacyPass::ID = 0;

INITIALIZE_PASS_BEGIN(SafeHeapLegacyPass, DEBUG_TYPE,
    "Safe Heap instrumentation", false, false)
INITIALIZE_PASS_END(SafeHeapLegacyPass, DEBUG_TYPE,
    "Safe Heap instrumentation", false, false)

PreservedAnalyses SafeHeapPass::run(Function &F,
                                    FunctionAnalysisManager &AM) {
  // bool Changed = SafeHeap(F).run();
  // if (!Changed)
    return PreservedAnalyses::all();
  // PreservedAnalyses PA;
  // PA.preserve<DominatorTreeAnalysis>();
  // return PA;
}

FunctionPass *llvm::createSafeHeapPass() { return new SafeHeapLegacyPass(); }
