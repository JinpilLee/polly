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

using namespace llvm;

namespace llvm {
class Instruction;
} // namespace llvm

namespace polly {
class Scop;
class SPDIR;

class SPDInstr {
public:
  SPDInstr(Instruction *I) : LLVMInstr(I) {}

  bool isDeadInstr() const {
    return UserList.empty() | LLVMInstr->mayWriteToMemory();
  }

  void dump() const;

private:
  Instruction *LLVMInstr;
  std::vector<SPDInstr *> UserList;

  SPDInstr() = delete;
};

class SPDIR {
public:
  SPDIR(Scop &S);

  ~SPDIR() {
    for (SPDInstr *I : InstrList) {
      delete I;
    }
  }

  void dump() const;

private:
  std::vector<SPDInstr *> InstrList;

  void removeDeadInstrs();
};
} // end namespace polly

#endif // POLLY_SPD_IR_H
