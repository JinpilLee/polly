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

#include "polly/LinkAllPasses.h"
#include "polly/HostCodeGeneration.h"
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

bool HostCodeGeneration::runOnFunction(Function &F) {
  ScopInfo *SI = getAnalysis<ScopInfoWrapperPass>().getSI();
  bool Changed = false;

  if (MDNode *Node = F.getMetadata("polly_extracted_loop")) {
    ValueAsMetadata *VM = dyn_cast<ValueAsMetadata>(Node->getOperand(0));
    const Scop *S
      = getScopFromInstr(dyn_cast<Instruction>(VM->getValue()), SI);
// FIXME for test
    std::cerr << "Scop INFO --------------------------------\n";
    S->dump();
    std::cerr << "Scop INFO END ----------------------------\n";

    // FIXME consider better impl than using counter
    unsigned InstCount = 0;
    for (auto UI = F.use_begin(), UE = F.use_end(); UI != UE;) {
      Use *U = &*UI;
      ++UI;
      ++InstCount;
      CallInst *Caller = dyn_cast<CallInst>(U->getUser());
      assert(Caller != nullptr && "user should be a function call");

// -----------------------------------
// FIXME return type sould be void
      Module *M = F.getParent();
      Type *RetTy = Type::getVoidTy(M->getContext());
      Type *ArgTy = Type::getInt32Ty(M->getContext());
      IRBuilder<> Builder(Caller);

      Value *RuntimeFunc = nullptr;
      RuntimeFunc = M->getOrInsertFunction("__spd_copy_in",
                                           RetTy,
                                           ArgTy, ArgTy);
      Builder.CreateCall(RuntimeFunc,
                         {ConstantInt::get(ArgTy, 0),
                          ConstantInt::get(ArgTy, 1)});

      RuntimeFunc = M->getOrInsertFunction("__spd_run_kernel",
                                           RetTy,
                                           ArgTy, ArgTy, ArgTy);
      Builder.CreateCall(RuntimeFunc,
                         {ConstantInt::get(ArgTy, 0),
                          ConstantInt::get(ArgTy, 1),
                          ConstantInt::get(ArgTy, 2)});

      RuntimeFunc = M->getOrInsertFunction("__spd_copy_out",
                                           RetTy,
                                           ArgTy);
      Builder.CreateCall(RuntimeFunc,
                         {ConstantInt::get(ArgTy, 0)});

      Caller->eraseFromParent();
// -----------------------------------

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
