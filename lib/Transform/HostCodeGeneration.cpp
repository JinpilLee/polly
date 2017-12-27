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

static void createCopyInFunc(SPDIR &IR, Module &M, IRBuilder<> &IRB) {
  Type *RetTy = Type::getVoidTy(M.getContext());
  Type *FloatPtrTy = Type::getFloatPtrTy(M.getContext());
  Type *Int32Ty = Type::getInt32Ty(M.getContext());
  Type *Int64Ty = Type::getInt64Ty(M.getContext());

  Value *Func = nullptr;
  int StreamNumDims = IR.getStreamNumDims();
  switch (StreamNumDims) {
  default:
    llvm_unreachable("Stream must be 1D/2D/3D");
  case 1:
    Func = M.getOrInsertFunction("__spd_copy_in_1D", RetTy,
                                 Int32Ty,
                                 FloatPtrTy, Int64Ty);
    break;
  case 2:
    Func = M.getOrInsertFunction("__spd_copy_in_2D", RetTy,
                                 Int32Ty,
                                 FloatPtrTy, Int64Ty, Int64Ty);
    break;
  case 3:
    Func = M.getOrInsertFunction("__spd_copy_in_3D", RetTy,
                                 Int32Ty,
                                 FloatPtrTy, Int64Ty, Int64Ty, Int64Ty);
    break;
  }

  uint32_t Offset = 0;
  for (auto Iter = IR.read_begin(); Iter != IR.read_end(); Iter++) {
    SmallVector<Value *, 8> Args;

    Args.push_back(IRB.getInt32(Offset));

    SPDArrayInfo *AI = *Iter;
    Value *ArrayRef = AI->getArrayRef();
    ArrayRef = IRB.CreatePointerCast(ArrayRef, FloatPtrTy);
    Args.push_back(ArrayRef);
    for (uint64_t DimSize : *AI) {
      Args.push_back(IRB.getInt64(DimSize));
    }

    Value *Ref = AI->getArrayRef();
    IRB.CreateCall(Func, Args);
    Offset++;
  }
}

static void createCopyOutFunc(SPDIR &IR, Module &M, IRBuilder<> &Builder) {
  Type *RetTy = Type::getVoidTy(M.getContext());
  Type *ArgTy = Type::getInt32Ty(M.getContext());
  Value *RuntimeFunc = nullptr;

  RuntimeFunc = M.getOrInsertFunction("__spd_run_kernel",
                                      RetTy,
                                      ArgTy, ArgTy, ArgTy);

  Builder.CreateCall(RuntimeFunc,
                     {ConstantInt::get(ArgTy, 0),
                      ConstantInt::get(ArgTy, 1),
                      ConstantInt::get(ArgTy, 2)});
}

static void createRunKernelFunc(SPDIR &IR, Module &M, IRBuilder<> &Builder) {
  Type *RetTy = Type::getVoidTy(M.getContext());
  Type *ArgTy = Type::getInt32Ty(M.getContext());
  Value *RuntimeFunc = nullptr;

  RuntimeFunc = M.getOrInsertFunction("__spd_copy_out",
                                      RetTy,
                                      ArgTy);
 
  Builder.CreateCall(RuntimeFunc,
                     {ConstantInt::get(ArgTy, 0)});
}

bool HostCodeGeneration::runOnFunction(Function &F) {
  ScopInfo *SI = getAnalysis<ScopInfoWrapperPass>().getSI();
  bool Changed = false;

  if (MDNode *Node = F.getMetadata("polly_extracted_loop")) {
    ValueAsMetadata *VM = dyn_cast<ValueAsMetadata>(Node->getOperand(0));
    const Scop *S
      = getScopFromInstr(dyn_cast<Instruction>(VM->getValue()), SI);

    SPDIR IR(*S);

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

      createCopyInFunc(IR, *M, Builder);
      createCopyOutFunc(IR, *M, Builder);
      createRunKernelFunc(IR, *M, Builder);

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
