#include "polly/ScopInfo.h"
#include "polly/CodeGen/SPDPrinter.h"
#include <iostream>

using namespace llvm;
using namespace polly;

void SPDPrinter::emitInParams() {
  if (IR->getNumReads() == 0) return;

  *OS << "Main_In   {Mi::";
  auto Iter = IR->read_begin();
  while (true) {
    SPDArrayInfo *AI = *Iter;
    *OS << AI->getArrayRef()->getName().str();

    Iter++;
    if (Iter == IR->read_end()) {
      *OS << ", sop, eop};\n";
      break;
    }
    else {
      *OS << ", ";
    }
  }
}

void SPDPrinter::emitOutParams() {
  if (IR->getNumWrites() == 0) return;

  *OS << "Main_Out  {Mo::";
  auto Iter = IR->write_begin();
  while (true) {
    SPDArrayInfo *AI = *Iter;
    *OS << AI->getArrayRef()->getName().str();

    Iter++;
    if (Iter == IR->write_end()) {
      *OS << ", sop, eop};\n";
      break;
    }
    else {
      *OS << ", ";
    }
  }
}

void SPDPrinter::emitModuleDecl(std::string &KernelName) {
// FIXME needs name
  *OS << "Name      " << KernelName << ";\n";

  emitInParams();
  emitOutParams();
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

// copied from WriteConstantInternal()@IR/AsmPrinter.cpp
void SPDPrinter::emitConstantFP(ConstantFP *CFP) {
  if (&CFP->getValueAPF().getSemantics() == &APFloat::IEEEsingle() ||
      &CFP->getValueAPF().getSemantics() == &APFloat::IEEEdouble()) {
    bool ignored;
    bool isDouble = &CFP->getValueAPF().getSemantics()==&APFloat::IEEEdouble();
    bool isInf = CFP->getValueAPF().isInfinity();
    bool isNaN = CFP->getValueAPF().isNaN();
    if (!isInf && !isNaN) {
      double Val = isDouble ? CFP->getValueAPF().convertToDouble() :
                              CFP->getValueAPF().convertToFloat();
      SmallString<128> StrVal;
      raw_svector_ostream(StrVal) << Val;

      if ((StrVal[0] >= '0' && StrVal[0] <= '9') ||
          ((StrVal[0] == '-' || StrVal[0] == '+') &&
           (StrVal[1] >= '0' && StrVal[1] <= '9'))) {
        if (APFloat(APFloat::IEEEdouble(), StrVal).convertToDouble() == Val) {
          *OS << StrVal;
          return;
        }
      }
    }
    static_assert(sizeof(double) == sizeof(uint64_t),
                  "assuming that double is 64 bits!");
    APFloat apf = CFP->getValueAPF();
    if (!isDouble)
      apf.convert(APFloat::IEEEdouble(), APFloat::rmNearestTiesToEven,
                        &ignored);
    *OS << format_hex(apf.bitcastToAPInt().getZExtValue(), 0, /*Upper=*/true);
    return;
  }

  *OS << "0x";
  APInt API = CFP->getValueAPF().bitcastToAPInt();
  if (&CFP->getValueAPF().getSemantics() == &APFloat::x87DoubleExtended()) {
    *OS << 'K';
    *OS << format_hex_no_prefix(API.getHiBits(16).getZExtValue(), 4,
                               /*Upper=*/true);
    *OS << format_hex_no_prefix(API.getLoBits(64).getZExtValue(), 16,
                               /*Upper=*/true);
  }
  else if (&CFP->getValueAPF().getSemantics() == &APFloat::IEEEquad()) {
    *OS << 'L';
    *OS << format_hex_no_prefix(API.getLoBits(64).getZExtValue(), 16,
                               /*Upper=*/true);
    *OS << format_hex_no_prefix(API.getHiBits(64).getZExtValue(), 16,
                               /*Upper=*/true);
  }
  else if (&CFP->getValueAPF().getSemantics() == &APFloat::PPCDoubleDouble()) {
    *OS << 'M';
    *OS << format_hex_no_prefix(API.getLoBits(64).getZExtValue(), 16,
                               /*Upper=*/true);
    *OS << format_hex_no_prefix(API.getHiBits(64).getZExtValue(), 16,
                               /*Upper=*/true);
  }
  else if (&CFP->getValueAPF().getSemantics() == &APFloat::IEEEhalf()) {
    *OS << 'H';
    *OS << format_hex_no_prefix(API.getZExtValue(), 4,
                               /*Upper=*/true);
  }
  else {
    llvm_unreachable("unsupported floating point type");
  }
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

void SPDPrinter::emitValue(Value *V) {
  MemInstrMapTy::iterator Iter = MemInstrMap.find(V);
  if (Iter != MemInstrMap.end()) {
    // FIXME we need to consider array subscript
    MemoryAccess *MA = Iter->second;
    *OS << MA->getOriginalBaseAddr()->getName().str();
  }
  else if (isa<ConstantInt>(V)) {
    emitConstantInt(dyn_cast<ConstantInt>(V));
  }
  else if (isa<ConstantFP>(V)) {
    emitConstantFP(dyn_cast<ConstantFP>(V));
  }
  else {
    if (V->hasName()) {
      *OS << V->getName().str();
    }
    else {
      *OS << getValueNum(V);
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
    *OS << "EQU       equ" << EQUCount << ", ";
    EQUCount++;
}

void SPDPrinter::emitInstruction(SPDInstr *I) {
  Instruction *Instr = I->getLLVMInstr();
  if (Instr->mayReadFromMemory()) {
    MemInstrMap[dyn_cast<Value>(Instr)] = I->getMemoryAccess();
  }
  else if (Instr->mayWriteToMemory()) {
    emitEQUPrefix();
    MemoryAccess *MA = I->getMemoryAccess();
    *OS << MA->getOriginalBaseAddr()->getName().str();
    *OS << " = ";
    emitValue(Instr->getOperand(0));
    *OS << ";\n";
  }
  else if (Instr->isBinaryOp()) {
    emitEQUPrefix();
    emitValue(dyn_cast<Value>(Instr));
    *OS << " = ";
    emitValue(Instr->getOperand(0));
    emitOpcode(Instr->getOpcode());
    emitValue(Instr->getOperand(1));
    *OS << ";\n";
  }
  else {
    llvm_unreachable("unsupported instruction");
  }
}

SPDPrinter::SPDPrinter(SPDIR *I)
  : IR(I), EQUCount(0), ValueCount(0) {
  std::error_code EC;
  std::string KernelName("kernel");
  KernelName += std::to_string(IR->getKernelNum());
  OS = new raw_fd_ostream(KernelName + ".spd", EC, sys::fs::F_None);
  if (EC) {
    std::cerr << "cannot create a output file";
  }

  emitModuleDecl(KernelName);

  for (auto Iter = IR->instr_begin(); Iter != IR->instr_end(); Iter++) {
    SPDInstr *Instr = *Iter;
    emitInstruction(Instr);
  }

  *OS << "DRCT (Mo::sop, Mo::eop) = (Mi::sop, Mi::eop);\n";
}

SPDPrinter::~SPDPrinter() {
  delete OS;
}
