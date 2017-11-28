//===--- LoopExtraction.h - Loop Extractor ----------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Loop Extractor
// - This pass generates a function from each specified loop.
// - LoopExtraction should be used before SCOP generation.
//
//===----------------------------------------------------------------------===//

#ifndef POLLY_LOOP_EXTRACTION_H
#define POLLY_LOOP_EXTRACTION_H

#include "llvm/Analysis/LoopPass.h"

using namespace llvm;

namespace llvm {
class Loop;
} // namespace llvm

namespace polly {
struct LoopExtraction : public LoopPass {
  static char ID;
  unsigned NumLoops;

  LoopExtraction(unsigned N = ~0)
    : LoopPass(ID), NumLoops(N) {}

  bool runOnLoop(Loop *L, LPPassManager &) override;
  void getAnalysisUsage(AnalysisUsage &AU) const override;
};
} // end namespace polly

namespace llvm {
void initializeLoopExtractionPass(llvm::PassRegistry &);
} // namespace llvm

#endif
