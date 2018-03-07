#include "polly/ScopInfo.h"
#include "polly/CodeGen/SPDPrinter.h"
#include <iostream>

using namespace llvm;
using namespace polly;

void SPDPrinter::emitInParams(uint64_t VL) {
  if (IR->getNumReads() == 0) return;

  *OS << "Main_In  {Mi::";
  auto Iter = IR->read_begin();
  while (true) {
    SPDArrayInfo *AI = *Iter;
    for (uint64_t i = 0; i < VL; i++) {
      *OS << AI->getArrayRef()->getName().str() << i << ", ";
    }

    Iter++;
    if (Iter == IR->read_end()) {
      *OS << "iattr, sop, eop};\n";
      break;
    }
  }
}

void SPDPrinter::emitOutParams(uint64_t VL) {
  if (IR->getNumWrites() == 0) return;

  *OS << "Main_Out {Mo::";
  auto Iter = IR->write_begin();
  while (true) {
    SPDArrayInfo *AI = *Iter;
    for (uint64_t i = 0; i < VL; i++) {
      *OS << AI->getArrayRef()->getName().str() << i << ", ";
    }

    Iter++;
    if (Iter == IR->write_end()) {
      *OS << "oattr, sop, eop};\n";
      break;
    }
  }
}

void SPDPrinter::emitModuleDecl(std::string &KernelName, uint64_t VL) {
// FIXME needs name
  *OS << "Name     " << KernelName << ";\n";

  emitInParams(VL);
  emitOutParams(VL);
}

// copied from WriteConstantInternal()@IR/AsmPrinter.cpp
void SPDPrinter::emitConstantInt(ConstantInt *CI) {
  if (CI->getType()->isIntegerTy(1)) {
    *OS << (CI->getZExtValue() ? "true" : "false");
  }
  else {
    *OS << CI->getValue();
  }
}

void SPDPrinter::emitConstantFP(ConstantFP *CFP) {
  const APFloat &APF = CFP->getValueAPF();
  if (&APF.getSemantics() == &APFloat::IEEEsingle()) {
    if (!(APF.isInfinity() || APF.isNaN())) {
      std::string FloatValueStr = std::to_string(APF.convertToFloat());
      for (auto Ch : FloatValueStr) {
        if (Ch == 'e') {
          return;
        }
        else if (Ch == '+') {
          continue;
        }
        else {
          *OS << Ch;
        }
      }

      return;
    }
  }

  llvm_unreachable("unsupported floating point type");
}

unsigned SPDPrinter::getValueNum(Value *V) {
  CalcInstrMapTy::iterator Iter = CalcInstrMap.find(V);
  if (Iter == CalcInstrMap.end()) {
    unsigned RetVal = CalcInstrMap[V] = ValueCount;
    ValueCount++;
    return RetVal;
  }
  else {
    return Iter->second;
  }
}

void SPDPrinter::emitValue(Value *V, uint64_t VL) {
  MemInstrMapTy::iterator Iter = MemInstrMap.find(V);
  if (Iter != MemInstrMap.end()) {
    SPDInstr *I = Iter->second;
    MemoryAccess *MA = I->getMemoryAccess();
    int64_t StreamOffset = I->getStreamOffset();
    assert((StreamOffset == 0) && "array subscript is not allowed here");
    *OS << MA->getOriginalBaseAddr()->getName().str() << VL;
  }
  else if (isa<ConstantInt>(V)) {
    emitConstantInt(dyn_cast<ConstantInt>(V));
  }
  else if (isa<ConstantFP>(V)) {
    emitConstantFP(dyn_cast<ConstantFP>(V));
  }
  else {
    if (V->hasName()) {
      *OS << V->getName().str() << VL;
    }
    else {
// FIXME requires unique prefix
      *OS << "xxxv" << getValueNum(V) << VL;
    }
  }
}

void SPDPrinter::emitOpcode(unsigned Opcode) {
  *OS << " ";
  switch (Opcode) {
  case Instruction::Add:
  case Instruction::FAdd:
    *OS << "+"; break;
  case Instruction::Sub:
  case Instruction::FSub:
    *OS << "-"; break;
  case Instruction::Mul:
  case Instruction::FMul:
    *OS << "*"; break;
  case Instruction::UDiv:
  case Instruction::SDiv:
  case Instruction::FDiv:
    *OS << "/"; break;
  default:
    llvm_unreachable("unsupported opcode");
  }
  *OS << " ";
}

void SPDPrinter::emitEQUPrefix() {
  *OS << "EQU      equ" << EQUCount << ", ";
  EQUCount++;
}

void SPDPrinter::emitHDLPrefix() {
  *OS << "HDL      hdl" << HDLCount << ", ";
  HDLCount++;
}

static Value *getUniqueMemRead(Value *V, const ScopStmt *Stmt) {
  Instruction *Instr = dyn_cast<Instruction>(V);
  if (Instr == nullptr) {
    return nullptr;
  }

  if (Instr->mayReadFromMemory()) {
    MemoryAccess *MA = Stmt->getArrayAccessOrNULLFor(Instr);
    return MA->getOriginalBaseAddr();
  }

  Value *Ret = nullptr;
  for (unsigned i = 0; i < Instr->getNumOperands(); i++) {
    if (Ret == nullptr) {
      Ret = getUniqueMemRead(Instr->getOperand(i), Stmt);
    }
    else {
      Value *Temp = getUniqueMemRead(Instr->getOperand(i), Stmt);
      if (Temp == nullptr) continue;
      else if (Temp != Ret) return nullptr;
    }
  }

  return Ret;
}

void SPDPrinter::emitInstruction(SPDInstr *I, uint64_t VL) {
  Instruction *Instr = I->getLLVMInstr();
  if (Instr->mayReadFromMemory()) {
// FIXME need this?
    int64_t StreamOffset = I->getStreamOffset();
    if (StreamOffset == 0) {
      MemInstrMap[dyn_cast<Value>(Instr)] = I;
    }
    else {
      int64_t OffsetAbs
        = (StreamOffset > 0) ? StreamOffset : -StreamOffset;

      emitHDLPrefix();

      if (StreamOffset > 0) { // foward
        *OS << OffsetAbs + 2 << ", (";
      }
      else { // backward
        *OS << 2 << ", (";
      }

      emitValue(dyn_cast<Value>(Instr), VL);
      *OS << ")() = ";

      if (StreamOffset > 0) { // foward
        *OS << "mStreamForward(";
        MemoryAccess *MA = I->getMemoryAccess();
        *OS << MA->getOriginalBaseAddr()->getName().str() << VL;
        *OS << ", Mi::eop[0])(), <.pConstWord(0),.pFwdCycles("
            << OffsetAbs << ")>;\n";
      }
      else { // backward
        *OS << "mStreamBackward(";
        MemoryAccess *MA = I->getMemoryAccess();
        *OS << MA->getOriginalBaseAddr()->getName().str() << VL;
        *OS << ", Mi::eop[0])(), <.pConstWord(0),.pBwdCycles("
            << OffsetAbs << ")>;\n";
      }
    }
  }
  else if (Instr->mayWriteToMemory()) {
    emitEQUPrefix();
    MemoryAccess *MA = I->getMemoryAccess();
    *OS << MA->getOriginalBaseAddr()->getName().str() << VL;
    *OS << " = mux(";
// false value
// FIXME current implementation uses array read instead of original value
    Value *UniqueMemRead = getUniqueMemRead(Instr->getOperand(0),
                                            I->getStmt());
    if (UniqueMemRead == nullptr) {
      llvm_unreachable("cannot find a original value for masking output");
    }
    else {
      emitValue(UniqueMemRead, VL);
    }
// true value
    *OS << ", "; 
    emitValue(Instr->getOperand(0), VL);
// condition
// FIXME requires name check "attr"
//       more complex condition can improve coverage
    *OS << ", iattr[0]);\n";
  }
  else if (Instr->isBinaryOp()) {
    emitEQUPrefix();
    emitValue(dyn_cast<Value>(Instr), VL);
    *OS << " = ";
    emitValue(Instr->getOperand(0), VL);
    emitOpcode(Instr->getOpcode());
    emitValue(Instr->getOperand(1), VL);
    *OS << ";\n";
  }
  else {
    llvm_unreachable("unsupported instruction");
  }
}

void SPDPrinter::emitUnrollModule(std::string &UnrolledKernelName,
                                  std::string &KernelName,
                                  uint64_t VL, uint64_t UC) {
// DECL part
  emitModuleDecl(UnrolledKernelName, VL);

// EQU Part
  if (IR->getNumReads() != IR->getNumWrites()) {
    llvm_unreachable("number of read/write arrays are not equal");
  }

  assert((VL > 0) && "VL should be more than 0");

// FIXME
// +3 is for attr, sop, eop
// remove this magic number when attr becomes optional
  int SurfixInc = (IR->getNumWrites() * VL) + 3;
  int SurfixStart = 0;
  for (uint64_t i = 0; i < UC; i++) {
// writes
    int WriteId = SurfixStart;
    *OS << "HDL      core" << i << ", ###, (";
    for (auto Iter = IR->write_begin();
         Iter != IR->write_end(); Iter++) {
      SPDArrayInfo *AI = *Iter;
      for (uint64_t j = 0; j < VL; j++) {
        if (i == (UC - 1)) {
          *OS << AI->getArrayRef()->getName().str() << j;
        }
        else {
          *OS << "xxxt" << WriteId; WriteId++;
        }

        *OS << ", ";
      }
    }

    if (i == (UC - 1)) {
      *OS << "oattr, Mo::sop, Mo::eop) = ";
    }
    else {
      *OS << "xxxt" << WriteId << ", "; WriteId++; // attr
      *OS << "xxxt" << WriteId << ", "; WriteId++; // sop
      *OS << "xxxt" << WriteId << ") = ";     // eop
    }

// reads
    int ReadId = SurfixStart - SurfixInc;
    *OS << KernelName << "(";
    for (auto Iter = IR->read_begin();
         Iter != IR->read_end(); Iter++) {
      SPDArrayInfo *AI = *Iter;
      for (uint64_t j = 0; j < VL; j++) {
        if (i == 0) {
          *OS << AI->getArrayRef()->getName().str() << j;
        }
        else {
          *OS << "xxxt" << ReadId; ReadId++;
        }

        *OS << ", ";
      }
    }

    if (i == 0) {
      *OS << "iattr, Mi::sop, Mi::eop);\n";
    }
    else {
      *OS << "xxxt" << ReadId << ", "; ReadId++; // attr
      *OS << "xxxt" << ReadId << ", "; ReadId++; // sop
      *OS << "xxxt" << ReadId << ");\n";     // eop
    }

    SurfixStart += SurfixInc;
  }
}

SPDPrinter::SPDPrinter(SPDIR *I, uint64_t VL, uint64_t UC)
  : IR(I), EQUCount(0), HDLCount(0), ValueCount(0) {
  std::error_code EC;
  std::string KernelName("kernel");
  KernelName += std::to_string(IR->getKernelNum());
  OS = new raw_fd_ostream(KernelName + ".spd", EC, sys::fs::F_None);
  if (EC) {
    std::cerr << "cannot create a output file";
  }

  emitModuleDecl(KernelName, VL);

  for (auto Iter = IR->instr_begin(); Iter != IR->instr_end(); Iter++) {
    SPDInstr *Instr = *Iter;
    for (uint64_t i = 0; i < VL; i++) {
      emitInstruction(Instr, i);
    }
  }

// FIXME attr should be optional
  *OS <<
    "DRCT     (oattr, Mo::sop, Mo::eop) = (iattr, Mi::sop, Mi::eop);\n";

  delete OS;

  assert((UC > 0) && "UC should be greater than 0");
  if (UC > 1) {
    std::string UnrolledKernelName("UC");
    UnrolledKernelName += std::to_string(UC);
    UnrolledKernelName += "_" + KernelName;
    OS = new raw_fd_ostream(UnrolledKernelName + ".spd",
                            EC, sys::fs::F_None);

    emitUnrollModule(UnrolledKernelName, KernelName, VL, UC);

    delete OS;
  }
}
