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

#include <cstdint>
#include <vector>

namespace llvm {
class Instruction;
class Value;
} // namespace llvm

using namespace llvm;

namespace polly {
class MemoryAccess;
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
  Instruction *getLLVMInstr() const { return LLVMInstr; }
  MemoryAccess *getMemoryAccess() const;

private:
  Instruction *LLVMInstr;
  const ScopStmt *ParentStmt;
  const SPDIR *ParentIR;

  SPDInstr() = delete;
  SPDInstr(Instruction *I, const ScopStmt *Stmt, SPDIR *IR)
    : LLVMInstr(I), ParentStmt(Stmt), ParentIR(IR) {}
};

class SPDArrayInfo {
public:
  SPDArrayInfo(Value *V);

  bool equal(Value *V) const { return V == LLVMValue; }
  int getNumDims() const { return DimSizeList.size(); }
  Value *getArrayRef() const { return LLVMValue; }

  typedef std::vector<std::uint64_t>::const_iterator const_iterator;
  const_iterator begin() const { return DimSizeList.begin(); }
  const_iterator end() const { return DimSizeList.end(); }

  void dump() const;

private:
  Value *LLVMValue;
  std::vector<std::uint64_t> DimSizeList;
};

class SPDStreamInfo {
public:
  SPDStreamInfo(uint32_t NumArrays, int NumDims, uint64_t *L);

  uint32_t getStride() const { return Stride; }
  int getNumDims() const { return DimSizeList.size(); }
  uint64_t getAllocSize() const {
    uint64_t Size = 0;
    for (auto Iter = begin(); Iter != end(); Iter++) {
      Size *= *Iter;
    }

    return Size * getStride();
  }

  typedef std::vector<std::uint64_t>::const_iterator const_iterator;
  const_iterator begin() const { return DimSizeList.begin(); }
  const_iterator end() const { return DimSizeList.end(); }

private:
  uint32_t Stride;
  std::vector<std::uint64_t> DimSizeList;
};

class SPDIR {
public:
  SPDIR(const Scop &S);

  ~SPDIR() {
    for (SPDInstr *I : InstrList) {
      delete I;
    }

    for (SPDArrayInfo *AI : ReadAccesses) {
      delete AI;
    }

    for (SPDArrayInfo *AI : WriteAccesses) {
      delete AI;
    }

    delete SI;
  }

  bool has(Instruction *I) const;

  typedef std::vector<SPDInstr *>::const_iterator instr_iterator;
  instr_iterator instr_begin() const { return InstrList.begin(); }
  instr_iterator instr_end() const { return InstrList.end(); }

  typedef std::vector<SPDArrayInfo *>::const_iterator const_iterator;
  const_iterator read_begin() const { return ReadAccesses.begin(); };
  const_iterator read_end() const { return ReadAccesses.end(); };
  int getNumReads() const { return ReadAccesses.size(); }
  const_iterator write_begin() const { return WriteAccesses.begin(); };
  const_iterator write_end() const { return WriteAccesses.end(); };
  int getNumWrites() const { return WriteAccesses.size(); }

  typedef std::vector<std::uint64_t>::const_iterator stream_iterator;
  stream_iterator stream_begin() const { return SI->begin(); }
  stream_iterator stream_end() const { return SI->end(); }
  uint64_t getStreamAllocSize() const { return SI->getAllocSize(); }
  int getStreamStride() const { return SI->getStride(); }
  int getStreamNumDims() const { return SI->getNumDims(); }

  void dump() const;

private:
  std::vector<SPDInstr *> InstrList;
  std::vector<SPDArrayInfo *> ReadAccesses;
  std::vector<SPDArrayInfo *> WriteAccesses;
  SPDStreamInfo *SI;

  bool reads(Value *V) const;
  bool writes(Value *V) const;
  void addMemoryAccess(const MemoryAccess *MA);
  void createStreamInfo();
  void removeDeadInstrs();
};
} // end namespace polly

#endif // POLLY_SPD_IR_H
