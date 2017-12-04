//===- SPDCodeGen.cpp - SPD code generation -------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This path translates Scop/LLVM-IR into SPD.
//
//===----------------------------------------------------------------------===//

#include "polly/LinkAllPasses.h"
#include "polly/CodeGen/SPDCodeGen.h"
#include "polly/CodeGen/SPDIR.h"

#define DEBUG_TYPE "polly-ast"

using namespace llvm;
using namespace polly;

bool SPDCodeGen::runOnScop(Scop &S) {
  // Skip SCoPs in case they're already handled by PPCGCodeGeneration.
  // FIXME needed for SPDCodeGen?
  if (S.isToBeSkipped())
    return false;

  SPDIR IR(S);

  return false;
}

void SPDCodeGen::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<ScopInfoRegionPass>();
  AU.setPreservesAll();
}

char SPDCodeGen::ID = 0;

Pass *polly::createSPDCodeGenPass() {
  return new SPDCodeGen();
}

INITIALIZE_PASS_BEGIN(SPDCodeGen, "polly-spdcodegen",
                      "Polly - Generate SPD from Scop", false, false);
INITIALIZE_PASS_DEPENDENCY(ScopInfoRegionPass);
INITIALIZE_PASS_END(SPDCodeGen, "polly-spdcodegen",
                    "Polly - Generate SPD from Scop", false, false)
