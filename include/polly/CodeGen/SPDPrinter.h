//===- SPDPrinter.h - SPD file printer --------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This path emits SPD file onto screen
// FIXME this is a temporary implementation.
//
//===----------------------------------------------------------------------===//

#ifndef POLLY_SPD_PRINTER
#define POLLY_SPD_PRINTER

#include "llvm/ADT/APInt.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/raw_ostream.h"
#include "polly/CodeGen/SPDIR.h"
#include <map>
#include <string>

using namespace llvm;

namespace polly {

typedef std::map<Value *, unsigned>       CalcInstrMapTy;
typedef std::map<Value *, MemoryAccess *> MemInstrMapTy;

class SPDPrinter {
public:
  SPDPrinter(SPDIR *I, uint64_t VL);
  ~SPDPrinter();

// FIXME
// unsigned getLatency();

private:
  SPDPrinter() = delete;
  void emitInParams(uint64_t VL);
  void emitOutParams(uint64_t VL);
  void emitModuleDecl(std::string &KernelName, uint64_t VL);
  void emitConstantInt(ConstantInt *CI);
  void emitConstantFP(ConstantFP *CFP);
  unsigned getValueNum(Value *V);
  void emitValue(Value *V, uint64_t VL);
  void emitOpcode(unsigned Opcode);
  void emitEQUPrefix();
  void emitInstruction(SPDInstr *Instr, uint64_t VL);

  raw_fd_ostream *OS;
  SPDIR *IR;
  uint64_t VectorLength;

  unsigned EQUCount;
  unsigned HDLCount;
  unsigned ValueCount;
  CalcInstrMapTy CalcInstrMap;
  MemInstrMapTy MemInstrMap;
};
} // namespace polly

#endif // POLLY_SPD_PRINTER
