//===--- HostCodeGeneration.h - Host Code Generator -------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Host Code Generator
// - This pass generates host-side code for FPGA
//   - invokes FPGA kernels
//   - transfer data between FPGA and host memory
//
//===----------------------------------------------------------------------===//

#ifndef POLLY_HOST_CODE_GENERATION_H
#define POLLY_HOST_CODE_GENERATION_H

#include "llvm/IR/Function.h"
#include "llvm/IR/PassManager.h"
#include <map>

namespace llvm {
class Instruction;
class Function;
} // namespace llvm

using namespace llvm;

namespace polly {
class Scop;
class ScopInfo;

struct HostCodeGeneration : public FunctionPass {
  static char ID;

  HostCodeGeneration() : FunctionPass(ID) {}

  bool doInitialization(Module &M) override;
  bool runOnFunction(Function &F) override;
  void getAnalysisUsage(AnalysisUsage &AU) const override;

private:
  std::map<uint64_t, Instruction *> RegionBeginMap;
  std::map<uint64_t, Instruction *> RegionEndMap;

  uint64_t getRegionNumber(Instruction *Instr) const;
  const Scop *getScopFromInstr(Instruction *Instr, ScopInfo *SI) const;
};
} // end namespace polly

namespace llvm {
void initializeHostCodeGenerationPass(llvm::PassRegistry &);
} // namespace llvm

#endif // POLLY_HOST_CODE_GENERATION_H
