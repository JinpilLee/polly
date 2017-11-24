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

// FIXME for test
#include <iostream>

#define DEBUG_TYPE "polly-ast"

using namespace llvm;
using namespace polly;

bool SPDCodeGen::runOnScop(Scop &Scop) {
  // Skip SCoPs in case they're already handled by PPCGCodeGeneration.
  // FIXME needed for SPDCodeGen?
  if (Scop.isToBeSkipped())
    return false;

  for (ScopStmt &Stmt : Scop) {
    if (Stmt.isBlockStmt()) {
      BasicBlock *BB = Stmt.getBasicBlock();
      for (const Instruction &Instr : *BB) {
        if (Instr.mayReadOrWriteMemory()) {
          MemoryAccess *MA = Stmt.getArrayAccessOrNULLFor(&Instr);
          if (MA != nullptr) {
            std::cerr << "MEM ACC -----------------------\n";
            MA->getOriginalBaseAddr()->dump();
            unsigned Num = MA->getNumSubscripts();
            for (unsigned i = 0; i < Num; i++) {
              MA->getSubscript(i)->dump();
            }
            // FIXME this is a {read or write} instruction.
            // generate SPD from MemoryAccess (Scop)
          }
          else {
            std::cerr << "????? -------------------------\n";
            // FIXME how can i handle this case???
          }
        }
        else {
          std::cerr << "INSTR -------------------------\n";
          // FIXME use SPDPrinter
          Instr.dump();
        }
      }
    }
  }

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
