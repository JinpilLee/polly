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
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"

using namespace llvm;
using namespace polly;

#define DEBUG_TYPE "polly-host-codegen"

// FIXME for test
#include <iostream>

bool HostCodeGeneration::runOnFunction(Function &F) {
  bool Changed = false;

  if (F.getMetadata("polly_extracted_loop")) {
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
}

char HostCodeGeneration::ID = 0;

Pass *polly::createHostCodeGenerationPass() {
  return new HostCodeGeneration();
}

INITIALIZE_PASS_BEGIN(HostCodeGeneration, "polly-host-codegen",
                      "Polly - Host Code Generation", false, false);
INITIALIZE_PASS_END(HostCodeGeneration, "polly-host-codegen",
                    "Polly - Host Code Generation", false, false)
