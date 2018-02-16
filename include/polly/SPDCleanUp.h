//===--- SPDCleanUp.h - SPD Clean Up ----------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// SPD Clean Up Pass
//
//===----------------------------------------------------------------------===//

#ifndef POLLY_SPD_CLEAN_UP_H
#define POLLY_SPD_CLEAN_UP_H

#include "llvm/IR/PassManager.h"

namespace llvm {
class Function;
} // namespace llvm

using namespace llvm;

namespace polly {
struct SPDCleanUp : public FunctionPass {
  static char ID;

  SPDCleanUp() : FunctionPass(ID) {}

  bool runOnFunction(Function &F) override;
  void getAnalysisUsage(AnalysisUsage &AU) const override;
};
} // end namespace polly

namespace llvm {
void initializeSPDCleanUpPass(llvm::PassRegistry &);
} // namespace llvm

#endif // POLLY_SPD_CLEAN_UP_H
