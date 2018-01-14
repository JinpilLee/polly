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
  SPDArrayInfo(Value *V, int O);

  int getOffset() const { return Offset; }
  bool equal(Value *V) const { return V == LLVMValue; }
  int getNumDims() const { return DimSizeList.size(); }
  Value *getArrayRef() const { return LLVMValue; }

  typedef std::vector<std::uint64_t>::const_iterator const_iterator;
  const_iterator begin() const { return DimSizeList.begin(); }
  const_iterator end() const { return DimSizeList.end(); }

  void dump() const;

private:
  int Offset;
  Value *LLVMValue;
  std::vector<std::uint64_t> DimSizeList;
};

class SPDStreamInfo {
public:
  SPDStreamInfo(uint32_t NumArrays, int NumDims, uint64_t *L);

  uint32_t getStride() const { return Stride; }
  int getNumDims() const { return DimSizeList.size(); }
  uint64_t getAllocSize() const {
    uint64_t Size = 1;
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

    delete ReadStream;
    delete WriteStream;
  }

  int getKernelNum() const { return KernelNum; }

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

  SPDStreamInfo *getReadStream() const { return ReadStream; }
  SPDStreamInfo *getWriteStream() const { return WriteStream; }

  void dump() const;

private:
  int KernelNum;
  std::vector<SPDInstr *> InstrList;
  std::vector<SPDArrayInfo *> ReadAccesses;
  std::vector<SPDArrayInfo *> WriteAccesses;
  SPDStreamInfo *ReadStream;
  SPDStreamInfo *WriteStream;

  bool reads(Value *V) const;
  bool writes(Value *V) const;
  void addReadAccess(const MemoryAccess *MA, int &Offset);
  void addWriteAccess(const MemoryAccess *MA, int &Offset);
  void createReadStreamInfo();
  void createWriteStreamInfo();
  void removeDeadInstrs();
};
} // end namespace polly

#endif // POLLY_SPD_IR_H
