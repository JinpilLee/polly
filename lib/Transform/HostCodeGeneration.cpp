//===- HostCodeGeneration.cpp - generate host-side code for FPGA ----------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass generates a function from each specified loop.
//
//===----------------------------------------------------------------------===//

#include "polly/CodeGen/SPDIR.h"
#include "polly/CodeGen/SPDPrinter.h"
#include "polly/HostCodeGeneration.h"
#include "polly/LinkAllPasses.h"
#include "polly/ScopInfo.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"

// FIXME for test
#include <iostream>

using namespace llvm;
using namespace polly;

#define DEBUG_TYPE "polly-host-codegen"

static void generateSPDFromIR(SPDIR &IR) {
  ;
}

static Value *createAllocStreamFunc(SPDIR &IR, Module &M, IRBuilder<> &IRB) {
  Type *RetTy = Type::getFloatPtrTy(M.getContext());
  Type *Int64Ty = Type::getInt64Ty(M.getContext());
  Value *Func = M.getOrInsertFunction("__spd_alloc_stream", RetTy, Int64Ty);
  return IRB.CreateCall(Func, {IRB.getInt64(IR.getStreamAllocSize())});
}

static void createPackFunc(SPDIR &IR, Module &M, IRBuilder<> &IRB,
                           Value *StreamBuffer) {
  Type *RetTy = Type::getVoidTy(M.getContext());
  Type *FloatPtrTy = Type::getFloatPtrTy(M.getContext());
  Type *Int32Ty = Type::getInt32Ty(M.getContext());
  Type *Int64Ty = Type::getInt64Ty(M.getContext());

  Value *Func
    = M.getOrInsertFunction("__spd_pack_contiguous", RetTy,
                            FloatPtrTy, Int32Ty, Int32Ty,
                            FloatPtrTy, Int64Ty);

  for (auto Iter = IR.read_begin(); Iter != IR.read_end(); Iter++) {
    SPDArrayInfo *AI = *Iter;

    SmallVector<Value *, 8> Args;

    Args.push_back(StreamBuffer);
    Args.push_back(IRB.getInt32(AI->getOffset()));
    Args.push_back(IRB.getInt32(IR.getStreamStride()));

    Value *ArrayRef = AI->getArrayRef();
    ArrayRef = IRB.CreatePointerCast(ArrayRef, FloatPtrTy);
    Args.push_back(ArrayRef);

    uint64_t TotalSize = 1;
    for (uint64_t DimSize : *AI) {
      TotalSize *= DimSize;
    }
    Args.push_back(IRB.getInt64(TotalSize));

    IRB.CreateCall(Func, Args);
  }
}

static void createRunKernelFunc(SPDIR &IR, Module &M, IRBuilder<> &IRB) {
  Type *RetTy = Type::getVoidTy(M.getContext());
  Type *ArgTy = Type::getInt32Ty(M.getContext());
  Value *Func = nullptr;

  Func = M.getOrInsertFunction("__spd_run_kernel",
                               RetTy, ArgTy);

  SmallVector<Value *, 8> Args;
  Args.push_back(IRB.getInt32(IR.getKernelNum()));
  IRB.CreateCall(Func, Args);
}

static void createUnpackFunc(SPDIR &IR, Module &M, IRBuilder<> &IRB,
                             Value *StreamBuffer) {
  Type *RetTy = Type::getVoidTy(M.getContext());
  Type *FloatPtrTy = Type::getFloatPtrTy(M.getContext());
  Type *Int32Ty = Type::getInt32Ty(M.getContext());
  Type *Int64Ty = Type::getInt64Ty(M.getContext());

  Value *Func
    = M.getOrInsertFunction("__spd_unpack_contiguous", RetTy,
                            FloatPtrTy, Int64Ty,
                            FloatPtrTy, Int32Ty, Int32Ty);

  for (auto Iter = IR.write_begin(); Iter != IR.write_end(); Iter++) {
    SPDArrayInfo *AI = *Iter;

    SmallVector<Value *, 8> Args;

    Value *ArrayRef = AI->getArrayRef();
    ArrayRef = IRB.CreatePointerCast(ArrayRef, FloatPtrTy);
    Args.push_back(ArrayRef);

    uint64_t TotalSize = 1;
    for (uint64_t DimSize : *AI) {
      TotalSize *= DimSize;
    }
    Args.push_back(IRB.getInt64(TotalSize));

    Args.push_back(StreamBuffer);
    Args.push_back(IRB.getInt32(AI->getOffset()));
    Args.push_back(IRB.getInt32(IR.getStreamStride()));

    IRB.CreateCall(Func, Args);
  }
}

static void createFreeStreamFunc(SPDIR &IR, Module &M, IRBuilder<> &IRB,
                                 Value *StreamBuffer) {
  Type *RetTy = Type::getVoidTy(M.getContext());
  Type *FloatPtrTy = Type::getFloatPtrTy(M.getContext());

  Value *Func = M.getOrInsertFunction("__spd_free_stream", RetTy, FloatPtrTy);
  IRB.CreateCall(Func, {StreamBuffer});
}

const Scop *HostCodeGeneration::getScopFromInstr(Instruction *Instr,
                                                 ScopInfo *SI) const {
  BasicBlock *BB = Instr->getParent();
  for (auto &It : *SI) {
    Region *R = It.first;
    if (R->contains(BB)) {
      return It.second.get();
    }
  }

  llvm_unreachable("cannot find a scop");
}

bool HostCodeGeneration::runOnFunction(Function &F) {
  ScopInfo *SI = getAnalysis<ScopInfoWrapperPass>().getSI();
  bool Changed = false;

  if (MDNode *Node = F.getMetadata("polly_extracted_loop")) {
    ValueAsMetadata *VM = dyn_cast<ValueAsMetadata>(Node->getOperand(0));
    const Scop *S
      = getScopFromInstr(dyn_cast<Instruction>(VM->getValue()), SI);

    SPDIR IR(*S);
    SPDPrinter Print(&IR);

    // FIXME consider better impl than using counter
    unsigned InstCount = 0;
    for (auto UI = F.use_begin(), UE = F.use_end(); UI != UE;) {
      Use *U = &*UI;
      ++UI;
      ++InstCount;
      CallInst *Caller = dyn_cast<CallInst>(U->getUser());
      assert(Caller != nullptr && "user should be a function call");

      Module *M = F.getParent();
      IRBuilder<> Builder(Caller);

      Value *StreamBuffer = createAllocStreamFunc(IR, *M, Builder);
      createPackFunc(IR, *M, Builder, StreamBuffer);
      createRunKernelFunc(IR, *M, Builder);
      createUnpackFunc(IR, *M, Builder, StreamBuffer);
      createFreeStreamFunc(IR, *M, Builder, StreamBuffer);

      Caller->eraseFromParent();

      Changed = true;
    }
    assert(InstCount == 1 && "assuming one caller per extracted func");
  }

  return Changed;
}

void HostCodeGeneration::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<ScopInfoWrapperPass>();
  AU.addPreserved<ScopInfoWrapperPass>();
}

char HostCodeGeneration::ID = 0;

Pass *polly::createHostCodeGenerationPass() {
  return new HostCodeGeneration();
}

INITIALIZE_PASS_BEGIN(HostCodeGeneration, "polly-host-codegen",
                      "Polly - Host Code Generation", false, false);
INITIALIZE_PASS_DEPENDENCY(ScopInfoWrapperPass);
INITIALIZE_PASS_END(HostCodeGeneration, "polly-host-codegen",
                    "Polly - Host Code Generation", false, false)
