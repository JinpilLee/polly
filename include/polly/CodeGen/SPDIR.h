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

#ifndef POLLY_SPD_IR_H
#define POLLY_SPD_IR_H

#include <vector>

namespace llvm {
class Instruction;
} // namespace llvm

using namespace llvm;

namespace polly {
class Scop;
class ScopStmt;
class SPDIR;

class SPDInstr {
public:
  static SPDInstr *get(Instruction *I,
                       const ScopStmt *Stmt, SPDIR *IR);

  bool equal(Instruction *) const;
  bool isDeadInstr() const;
  void dump() const;

private:
  const Instruction *LLVMInstr;
  const ScopStmt *ParentStmt;
  const SPDIR *ParentIR;

  SPDInstr() = delete;
  SPDInstr(Instruction *I, const ScopStmt *Stmt, SPDIR *IR)
    : LLVMInstr(I), ParentStmt(Stmt), ParentIR(IR) {}
};

class SPDIR {
public:
  SPDIR(const Scop &S);

  ~SPDIR() {
    for (SPDInstr *I : InstrList) {
      delete I;
    }
  }

  bool has(Instruction *I) const;
  void dump() const;

private:
  std::vector<SPDInstr *> InstrList;

  void removeDeadInstrs();
};
} // end namespace polly

#endif // POLLY_SPD_IR_H
