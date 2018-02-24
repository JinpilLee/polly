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
#include <map>

namespace llvm {
class Instruction;
class Value;
class LoopInfo;
class ScalarEvolution;
} // namespace llvm

using namespace llvm;

namespace polly {
class MemoryAccess;
class Scop;
class ScopStmt;
class SPDIR;

// FIXME SPDInstr needs refactoring
// Implement SPDInstrMemRead/Write and move StreamOffset to them
class SPDInstr {
public:
  static SPDInstr *get(Instruction *I,
                       const ScopStmt *Stmt, SPDIR *IR);

  bool equal(Instruction *) const;
  bool isDeadInstr() const;
  void dump() const;
  Instruction *getLLVMInstr() const { return LLVMInstr; }
  MemoryAccess *getMemoryAccess() const;
  int64_t getStreamOffset() const { return StreamOffset; }

private:
  int64_t StreamOffset;
  Instruction *LLVMInstr;
  const ScopStmt *ParentStmt;
  const SPDIR *ParentIR;

  SPDInstr() = delete;
  SPDInstr(Instruction *I, const ScopStmt *Stmt, SPDIR *IR,
           int64_t SO)
    : StreamOffset(SO), LLVMInstr(I),
      ParentStmt(Stmt), ParentIR(IR) {}
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

class SPDDomainExpr {
public:
  SPDDomainExpr(uint64_t SI, uint64_t EI, uint64_t S)
    : StartIndex(SI), EndIndex(EI), Stride(S) {}

  uint64_t getStart() const { return StartIndex; }
  uint64_t getEnd() const { return EndIndex; }
  uint64_t getStride() const { return Stride; }

private:
  uint64_t StartIndex;
  uint64_t EndIndex;
  uint64_t Stride;
};

class SPDDomainInfo {
public:
  SPDDomainInfo(unsigned NumDims,
                uint64_t *StartList,
                uint64_t *EndList,
                uint64_t *StrideList) {
    for (unsigned i = 0; i < NumDims; i++) {
      DomainExprList.push_back(new SPDDomainExpr(StartList[i],
                                                 EndList[i],
                                                 StrideList[i]));
    }
  }

  ~SPDDomainInfo() {
    for (SPDDomainExpr *DE : *this) {
      delete DE;
    }
  }

  typedef std::vector<SPDDomainExpr *>::const_iterator const_iterator;
  const_iterator begin() const { return DomainExprList.begin(); }
  const_iterator end() const { return DomainExprList.end(); }

private:
  std::vector<SPDDomainExpr *> DomainExprList;
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
  SPDIR(const Scop &S, LoopInfo &LI, ScalarEvolution &SE);

  ~SPDIR() {
    delete DI;

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

  const SPDArrayInfo *getArrayInfo(Value *V) { return ArrayInfoTable[V]; }

  void dump() const;

private:
  int KernelNum;
  SPDDomainInfo *DI;
  std::vector<SPDInstr *> InstrList;
  std::vector<SPDArrayInfo *> ReadAccesses;
  std::vector<SPDArrayInfo *> WriteAccesses;
  SPDStreamInfo *ReadStream;
  SPDStreamInfo *WriteStream;
  std::map<Value *, SPDArrayInfo *> ArrayInfoTable;

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
