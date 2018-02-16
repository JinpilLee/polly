//===- SPDCleanUp.cpp - clean up for SPD ----------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// SPD Clean Up pass
//
//===----------------------------------------------------------------------===//

#include "polly/SPDCleanUp.h"
#include "polly/LinkAllPasses.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include <vector>

using namespace llvm;
using namespace polly;

#define DEBUG_TYPE "polly-spd-cleanup"

bool SPDCleanUp::runOnFunction(Function &F) {
  std::vector<Instruction *> RemoveInstrList;
  for (BasicBlock &BB : F) {
    for (BasicBlock::iterator Iter = BB.begin(); Iter != BB.end(); ) {
      Instruction *Instr = &*Iter++;
      CallInst *CI = dyn_cast<CallInst>(Instr);
      if (CI == nullptr) continue;

      Function *Func = CI->getCalledFunction();
      if (Func->getName().equals("__spd_begin") ||
          Func->getName().equals("__spd_end")) {
        RemoveInstrList.push_back(CI);
      }
    }
  }

  bool Changed = false;
  for (Instruction *Instr : RemoveInstrList) {
    Instr->eraseFromParent();
    Changed = true;
  }

  return Changed;
}

void SPDCleanUp::getAnalysisUsage(AnalysisUsage &AU) const {
}

char SPDCleanUp::ID = 0;

Pass *polly::createSPDCleanUpPass() {
  return new SPDCleanUp();
}

INITIALIZE_PASS_BEGIN(SPDCleanUp, "polly-spd-cleanup",
                      "Polly - SPD Clean Up", false, false);
INITIALIZE_PASS_END(SPDCleanUp, "polly-spd-cleanup",
                    "Polly - SPD Clean Up", false, false)
