//===- SPDCodeGen.h - SPD code generation -----------------------*- C++ -*-===//
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

#ifndef POLLY_SPD_CODE_GEN
#define POLLY_SPD_CODE_GEN

#include "polly/Config/config.h"
#include "polly/ScopPass.h"

namespace polly {
class Scop;
class MemoryAccess;

class SPDCodeGen : public ScopPass {
public:
  static char ID;
  SPDCodeGen() : ScopPass(ID) {}

  bool runOnScop(Scop &S) override;
  void getAnalysisUsage(AnalysisUsage &AU) const override;
};
} // namespace polly

namespace llvm {
void initializeSPDCodeGenPass(llvm::PassRegistry &);
} // namespace llvm

#endif // POLLY_SPD_CODE_GEN
