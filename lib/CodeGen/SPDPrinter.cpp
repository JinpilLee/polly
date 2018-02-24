#include "polly/ScopInfo.h"
#include "polly/CodeGen/SPDPrinter.h"
#include <iostream>

using namespace llvm;
using namespace polly;

void SPDPrinter::emitInParams(uint64_t VL) {
  if (IR->getNumReads() == 0) return;

  *OS << "Main_In   {Mi::";
  auto Iter = IR->read_begin();
  while (true) {
    SPDArrayInfo *AI = *Iter;
    for (uint64_t i = 0; i < VL; i++) {
      *OS << AI->getArrayRef()->getName().str() << i << ", ";
    }

    Iter++;
    if (Iter == IR->read_end()) {
      *OS << "attr, sop, eop};\n";
      break;
    }
  }
}

void SPDPrinter::emitOutParams(uint64_t VL) {
  if (IR->getNumWrites() == 0) return;

  *OS << "Main_Out  {Mo::";
  auto Iter = IR->write_begin();
  while (true) {
    SPDArrayInfo *AI = *Iter;
    for (uint64_t i = 0; i < VL; i++) {
      *OS << AI->getArrayRef()->getName().str() << i << ", ";
    }

    Iter++;
    if (Iter == IR->write_end()) {
      *OS << "attr, sop, eop};\n";
      break;
    }
  }
}

void SPDPrinter::emitModuleDecl(std::string &KernelName, uint64_t VL) {
// FIXME needs name
  *OS << "Name      " << KernelName << ";\n";

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
      *OS << "Val" << getValueNum(V) << VL;
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

static Value *getUniqueMemRead(Value *V, const ScopStmt *Stmt) {
  Instruction *Instr = dyn_cast<Instruction>(V);
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
      if (Temp != Ret) return nullptr;
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
      emitEQUPrefix();
      emitValue(dyn_cast<Value>(Instr), VL);
      *OS << " = ";
      MemoryAccess *MA = I->getMemoryAccess();
      *OS << MA->getOriginalBaseAddr()->getName().str() << VL;
      *OS << "<<" << StreamOffset << ">>\n";
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
    *OS << ", attr[0]);\n";
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

SPDPrinter::SPDPrinter(SPDIR *I, uint64_t VL, uint64_t UC)
  : IR(I), EQUCount(0), ValueCount(0) {
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
    "DRCT (Mo::attr, Mo::sop, Mo::eop) = (MI::attr, Mi::sop, Mi::eop);\n";

  delete OS;

  assert((UC > 0) && "UC should be greater than 0");
  if (UC > 1) {
    std::string UnrolledKernelName("UC");
    UnrolledKernelName += std::to_string(UC);
    UnrolledKernelName += "_" + KernelName;
    OS = new raw_fd_ostream(UnrolledKernelName + ".spd",
                            EC, sys::fs::F_None);

    emitModuleDecl(UnrolledKernelName, VL);
    for (uint64_t i = 0; i < UC; i++) {
      *OS << "EQU       equ" << i << ", ";
// FIXME complete here
      *OS << KernelName << "(###########)\n";
    }

    *OS <<
      "DRCT (Mo::attr, Mo::sop, Mo::eop) = (MI::attr, Mi::sop, Mi::eop);\n";

    delete OS;
  }
}

SPDPrinter::~SPDPrinter() {
}
