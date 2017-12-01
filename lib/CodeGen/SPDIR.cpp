//===--- SPDIR.h - SPD Intermediate Representation --------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// SPD Intermediate Representation
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/Instruction.h"
#include "llvm/IR/Value.h"
#include "polly/ScopInfo.h"
#include "polly/CodeGen/SPDIR.h"
#include "polly/CodeGen/SPDPrinter.h"

#include <iostream>

using namespace llvm;
using namespace polly;

void SPDInstr::dump() const {
  LLVMInstr->dump();
}

SPDIR::SPDIR(Scop &S) {
// FIXME for test
  for (ScopStmt &Stmt : S) {
    SPDPrinter Printer(&Stmt);
  }

  for (ScopStmt &Stmt : S) {
    BasicBlock *BB = Stmt.getBasicBlock();
    for (BasicBlock::iterator IIB = BB->begin(), IIE = BB->end();
         IIB != IIE; ++IIB) {
      Instruction &I = *IIB;
      InstrList.push_back(new SPDInstr(&I));
    }
  }

  removeDeadInstrs();
}

void SPDIR::removeDeadInstrs() {
}

void SPDIR::dump() const {
  std::cerr << "SPDIR::dump() ---------------------------\n";
  for (SPDInstr *I : InstrList) {
    I->dump();
  }
}
