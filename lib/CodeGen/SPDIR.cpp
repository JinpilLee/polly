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

#include "isl/map.h"
#include "isl/set.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Value.h"
#include "polly/ScopInfo.h"
#include "polly/CodeGen/SPDIR.h"
#include "polly/CodeGen/SPDPrinter.h"
#include <vector>

// FIXME for test
#include "isl/id.h"
#include <iostream>
#include <cstdio>

using namespace llvm;
using namespace polly;

static int KernelNumCount = 0;

SPDInstr *SPDInstr::get(Instruction *I,
                        const ScopStmt *Stmt, SPDIR *IR) {
  if (I->mayReadOrWriteMemory()) {
    MemoryAccess *MA = Stmt->getArrayAccessOrNULLFor(I);
    unsigned Num = MA->getNumSubscripts();
    for (unsigned i = 0; i < Num; i++) {
      const SCEVAddRecExpr *SExpr
        = dyn_cast<SCEVAddRecExpr>(MA->getSubscript(i));
      assert(SExpr->isAffine() &&
             "array subscripts should be expressed by an affine function");

// FIXME current impl only allows {0,+,1}<loop>
/*
      const SCEV *StartExpr = SExpr->getStart();
      assert(StartExpr->isZero() && "FIXME: currently StartExpr should be 0");
      const SCEV *StepExpr = SExpr->getStepRecurrence(*(Stmt->getParent()->getSE()));
      assert(StepExpr->isOne() && "FIXME: currently StepExpr should be 1");
*/
    }

    return new SPDInstr(I, Stmt, IR);
  }

  switch (I->getOpcode()) {
  default:
    return nullptr;
  case Instruction::Add:
  case Instruction::FAdd:
  case Instruction::Sub:
  case Instruction::FSub:
  case Instruction::Mul:
  case Instruction::FMul:
  case Instruction::UDiv:
  case Instruction::SDiv:
  case Instruction::FDiv:
    return new SPDInstr(I, Stmt, IR);
  }

  return nullptr;
}

bool SPDInstr::equal(Instruction *I) const {
  return LLVMInstr == I;
}

bool SPDInstr::isDeadInstr() const {
  if (LLVMInstr->mayWriteToMemory()) {
    return false;
  }

  for (auto UI = LLVMInstr->use_begin(),
       UE = LLVMInstr->use_end(); UI != UE;) {
    const Use *U = &*UI;
    ++UI;
    Instruction *UserInstr = dyn_cast<Instruction>(U->getUser());
    if (ParentIR->has(UserInstr)) {
      return false;
    }
  }

  return true;
}

void SPDInstr::dump() const {
  LLVMInstr->dump();
}

MemoryAccess *SPDInstr::getMemoryAccess() const {
  return ParentStmt->getArrayAccessOrNULLFor(LLVMInstr);
}

SPDArrayInfo::SPDArrayInfo(Value *V, int O) : Offset(O), LLVMValue(V) {
  if (!isa<GlobalVariable>(V)) {
    llvm_unreachable("MemoryAccess must be a global variable");
  }

  Type *T = V->getType(); 
  assert(T->isPointerTy() && "MemoryAccess must be a static array");
 
  T = T->getPointerElementType();
  if (!T->isArrayTy()) {
    llvm_unreachable("MemoryAccess must be a static array");
  }

  do {
    ArrayType *ATy = dyn_cast<ArrayType>(T);
    DimSizeList.insert(DimSizeList.begin(), ATy->getNumElements());
    T = ATy->getElementType();
  } while (T->isArrayTy());

  if (!T->isFloatTy()) {
    llvm_unreachable("MemoryAccess must be an array of float");
  }
}

void SPDArrayInfo::dump() const {
  LLVMValue->dump();
  for (int i = 0; i < getNumDims(); i++) {
    std::cerr << DimSizeList[i] << "\n";
  }
}

SPDStreamInfo::SPDStreamInfo(uint32_t NumArrays, int NumDims, uint64_t *L)
  : Stride(NumArrays) {
  for (int i = 0; i < NumDims; i++) {
    DimSizeList.push_back(L[i]);
  }
}

SPDIR::SPDIR(const Scop &S)
  : KernelNum(KernelNumCount) {
  KernelNumCount++;

// Analysis
  int Offset = 0;
  for (const ScopStmt &Stmt : S) {
    for (const MemoryAccess *MA : Stmt) {
      addReadAccess(MA, Offset);
    }
  }

  createReadStreamInfo();

  Offset = 0;
  for (const ScopStmt &Stmt : S) {
    for (const MemoryAccess *MA : Stmt) {
      addWriteAccess(MA, Offset);
    }
  }

  createWriteStreamInfo();

// FIXME temporary limitation
  if (ReadStream->getAllocSize() !=
      WriteStream->getAllocSize()) {
    llvm_unreachable("read/write stream should have the same size");
  }

// IR Generation
  for (const ScopStmt &Stmt : S) {
    BasicBlock *BB = Stmt.getBasicBlock();
    for (BasicBlock::iterator IIB = BB->begin(), IIE = BB->end();
         IIB != IIE; ++IIB) {
      Instruction &I = *IIB;
      SPDInstr *NewInstr = SPDInstr::get(&I, &Stmt, this);
      if (NewInstr != nullptr) {
        InstrList.push_back(NewInstr);
      }
    }
  }

  removeDeadInstrs();
}

bool SPDIR::has(Instruction *TargetInstr) const {
  for (SPDInstr *I : InstrList) {
    if (I->equal(TargetInstr)) {
      return true;
    }
  }

  return false;
}

void SPDIR::dump() const {
  std::cerr << "SPDIR::dump() ---------------------------\n";
  for (SPDInstr *I : InstrList) {
    I->dump();
  }
  std::cerr << "READ ---------------------------\n";
  for (SPDArrayInfo *AI : ReadAccesses) {
    AI->dump();
  }

  std::cerr << "WRITE --------------------------\n";
  for (SPDArrayInfo *AI : WriteAccesses) {
    AI->dump();
  }
}

bool SPDIR::reads(Value *V) const {
  for (SPDArrayInfo *R : ReadAccesses) {
    if (R->equal(V)) {
      return true;
    }
  }

  return false;
}

bool SPDIR::writes(Value *V) const {
  for (SPDArrayInfo *W : WriteAccesses) {
    if (W->equal(V)) {
      return true;
    }
  }

  return false;
}

void SPDIR::addReadAccess(const MemoryAccess *MA, int &Offset) {
  Value *BaseAddr = MA->getOriginalBaseAddr();
  if (MA->isRead()) {
    if (writes(BaseAddr)) {
      llvm_unreachable("READ and WRITE is not allowed");
    }
    else if (!reads(BaseAddr)) {
      ReadAccesses.push_back(new SPDArrayInfo(BaseAddr, Offset));
// FIXME bad impl
      Offset++;
    }
  }
}

void SPDIR::addWriteAccess(const MemoryAccess *MA, int &Offset) {
  Value *BaseAddr = MA->getOriginalBaseAddr();
  if (MA->isRead()) {
    // do nothing
    return;
  }
  else if (MA->isWrite()) {
    if (reads(BaseAddr)) {
      llvm_unreachable("READ and WRITE is not allowed");
    }
    else if (!writes(BaseAddr)) {
      WriteAccesses.push_back(new SPDArrayInfo(BaseAddr, Offset));
// FIXME bad impl
      Offset++;
    }
  }
  else {
    llvm_unreachable("MemoryAccess is not read/write");
  }
}

void SPDIR::createReadStreamInfo() {
  int NumDims = 0;
  for (auto Iter = read_begin(); Iter != read_end(); Iter++) {
    SPDArrayInfo *AI = *Iter;
    if (NumDims == 0) {
      NumDims = AI->getNumDims();
    }
    else {
      if (NumDims != AI->getNumDims()) {
        llvm_unreachable("Array dimension mush be the same");
      }
    }
  }

  uint64_t *DimSizeArray = new uint64_t[NumDims];
  for (int i = 0; i < NumDims; i++) {
    DimSizeArray[i] = 0;
  }

  uint32_t NumArrays = 0;
  for (auto Iter = read_begin(); Iter != read_end(); Iter++) {
    SPDArrayInfo *AI = *Iter;
    NumArrays++;
    int Idx = 0;
    for (uint64_t DimSize : *AI) {
      if (DimSize > DimSizeArray[Idx]) {
        DimSizeArray[Idx] = DimSize;
      }

      Idx++;
    }
  }

  ReadStream = new SPDStreamInfo(NumArrays, NumDims, DimSizeArray);
  delete[] DimSizeArray;
}

void SPDIR::createWriteStreamInfo() {
  int NumDims = 0;
  for (auto Iter = write_begin(); Iter != write_end(); Iter++) {
    SPDArrayInfo *AI = *Iter;
    if (NumDims == 0) {
      NumDims = AI->getNumDims();
    }
    else {
      if (NumDims!= AI->getNumDims()) {
        llvm_unreachable("Array dimension mush be the same");
      }
    }
  }

  uint64_t *DimSizeArray = new uint64_t[NumDims];
  for (int i = 0; i < NumDims; i++) {
    DimSizeArray[i] = 0;
  }

  uint32_t NumArrays = 0;
  for (auto Iter = write_begin(); Iter != write_end(); Iter++) {
    SPDArrayInfo *AI = *Iter;
    NumArrays++;
    int Idx = 0;
    for (uint64_t DimSize : *AI) {
      if (DimSize > DimSizeArray[Idx]) {
        DimSizeArray[Idx] = DimSize;
      }

      Idx++;
    }
  }

  WriteStream = new SPDStreamInfo(NumArrays, NumDims, DimSizeArray);
  delete[] DimSizeArray;
}

void SPDIR::removeDeadInstrs() {
  bool ContinueRemove = false;
  do {
    ContinueRemove = false;
    for (auto Iter = InstrList.begin(), IterEnd = InstrList.end();
         Iter != IterEnd; ++Iter) {
      SPDInstr *I = *Iter;
      if (I->isDeadInstr()) {
        InstrList.erase(Iter);
        ContinueRemove = true;
        break;
      }
    }
  } while (ContinueRemove);
}
