#include "llvm/ADT/APInt.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "SPDPrinter.h"

#include <iostream>
#include <map>

using namespace llvm;

typedef std::map<Value *, unsigned> ValueNumMapTy;

static unsigned EQUCount;
static unsigned HDLCount;
unsigned ValueCount;
ValueNumMapTy ValueNumMap;

// FIXME currently "fpga_main" is used to determine the entry function
// The OpenMP compiler for SPGen will generate a metadata node for
// a OMP Target directive, which can be used in this function.
bool isEntryFunction(Function &F) {
  std::string FunctionName = F.getName().str();
  if (FunctionName.compare("fpga_main") == 0) {
    return true;
  }

  return false;
}

static void emitInParams(raw_ostream &OS, Function &F) {
  if (F.arg_empty()) {
    if (isEntryFunction(F)) {
      OS << "Main_In   {Mi::__SPD_sop, __SPD_eop}\n";
    }

    return;
  }

  OS << "Main_In   {Mi::";
  unsigned ArgCount = 0;
  for (const Argument &A : F.args()) {
    if (ArgCount != 0) OS << ", ";
    OS << A.getName().str();
    ArgCount++;
  }

  if (isEntryFunction(F)) {
    OS << ", __SPD_sop, __SPD_eop";
  }

  OS << "};\n";
}

static void emitOutParams(raw_ostream &OS, Function &F) {
  bool HasReturnValue = false;
  Type *RetTy = F.getReturnType();
  switch (RetTy->getTypeID()) {
  case Type::VoidTyID:
    break;
  case Type::IntegerTyID:
  case Type::FloatTyID:
  case Type::DoubleTyID:
    HasReturnValue = true;
    break;
  default:
    llvm_unreachable("unsupported return type");
  }

  if (!HasReturnValue) {
    if (isEntryFunction(F)) {
      OS << "Main_Out  {Mo::__SPD_sop, __SPD_eop};\n";
    }

    return;
  }

  OS << "Main_Out  {Mo::";
  OS << "__SPD_ret";

  if (isEntryFunction(F)) {
    OS << ", __SPD_sop, __SPD_eop";
  }

  OS << "};\n";
}

static void emitFuncDecl(raw_ostream &OS, Function &F) {
  OS << "Name      " << F.getName().str() << ";\n";

  emitInParams(OS, F);
  emitOutParams(OS, F);
}

// copied from WriteConstantInternal()@IR/AsmPrinter.cpp
static void emitConstantInt(raw_ostream &OS, ConstantInt *CI) {
  if (CI->getType()->isIntegerTy(1)) {
    OS << (CI->getZExtValue() ? "true" : "false");
  }
  else {
    OS << CI->getValue();
  }
}

// copied from WriteConstantInternal()@IR/AsmPrinter.cpp
static void emitConstantFP(raw_ostream &OS, ConstantFP *CFP) {
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
          OS << StrVal;
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
    OS << format_hex(apf.bitcastToAPInt().getZExtValue(), 0, /*Upper=*/true);
    return;
  }

  OS << "0x";
  APInt API = CFP->getValueAPF().bitcastToAPInt();
  if (&CFP->getValueAPF().getSemantics() == &APFloat::x87DoubleExtended()) {
    OS << 'K';
    OS << format_hex_no_prefix(API.getHiBits(16).getZExtValue(), 4,
                               /*Upper=*/true);
    OS << format_hex_no_prefix(API.getLoBits(64).getZExtValue(), 16,
                               /*Upper=*/true);
  }
  else if (&CFP->getValueAPF().getSemantics() == &APFloat::IEEEquad()) {
    OS << 'L';
    OS << format_hex_no_prefix(API.getLoBits(64).getZExtValue(), 16,
                               /*Upper=*/true);
    OS << format_hex_no_prefix(API.getHiBits(64).getZExtValue(), 16,
                               /*Upper=*/true);
  }
  else if (&CFP->getValueAPF().getSemantics() == &APFloat::PPCDoubleDouble()) {
    OS << 'M';
    OS << format_hex_no_prefix(API.getLoBits(64).getZExtValue(), 16,
                               /*Upper=*/true);
    OS << format_hex_no_prefix(API.getHiBits(64).getZExtValue(), 16,
                               /*Upper=*/true);
  }
  else if (&CFP->getValueAPF().getSemantics() == &APFloat::IEEEhalf()) {
    OS << 'H';
    OS << format_hex_no_prefix(API.getZExtValue(), 4,
                               /*Upper=*/true);
  }
  else {
    llvm_unreachable("unsupported floating point type");
  }
}

static unsigned getValueNum(Value *V) {
  ValueNumMapTy::iterator Iter = ValueNumMap.find(V);
  if (Iter == ValueNumMap.end()) {
    unsigned RetVal = ValueNumMap[V] = ValueCount;
    ValueCount++;
    return RetVal;
  }
  else {
    return Iter->second;
  }
}

static void emitValue(raw_ostream &OS, Value *V) {
  if (isa<ConstantInt>(V)) {
    emitConstantInt(OS, dyn_cast<ConstantInt>(V));
  }
  else if (isa<ConstantFP>(V)) {
    emitConstantFP(OS, dyn_cast<ConstantFP>(V));
  }
  else {
    if (V->hasName()) {
      OS << V->getName().str();
    }
    else {
      OS << getValueNum(V);
    }
  }
}

static void emitOpcode(raw_ostream &OS, unsigned Opcode) {
  OS << " ";
  switch (Opcode) {
  case Instruction::Add:
  case Instruction::FAdd:
    OS << "+"; break;
  case Instruction::Sub:
  case Instruction::FSub:
    OS << "-"; break;
  case Instruction::Mul:
  case Instruction::FMul:
    OS << "*"; break;
  case Instruction::UDiv:
  case Instruction::SDiv:
  case Instruction::FDiv:
    OS << "/"; break;
  default:
    llvm_unreachable("unsupported opcode");
  }
  OS << " ";
}

static void emitEQUPrefix(raw_ostream &OS) {
    OS << "EQU       equ" << EQUCount << ", ";
    EQUCount++;
}

static void emitHDLPrefix(raw_ostream &OS, unsigned Delay) {
    OS << "HDL       hdl" << HDLCount << ", ";
    OS << Delay << ", ";
    HDLCount++;
}

static void emitInstruction(raw_ostream &OS, Instruction &Instr,
                            SPDModuleMapTy &SPDModuleMap) {
  unsigned NumOperands = Instr.getNumOperands();
  if (Instr.isBinaryOp()) {
    emitEQUPrefix(OS);
    emitValue(OS, dyn_cast<Value>(&Instr));
    OS << " = ";
    emitValue(OS, Instr.getOperand(0));
    emitOpcode(OS, Instr.getOpcode());
    emitValue(OS, Instr.getOperand(1));
    OS << ";\n";
  }
  else {
    switch (Instr.getOpcode()) {
    case Instruction::Call:
{
      std::string FuncName
        = Instr.getOperand(NumOperands - 1)->getName().str();
      SPDModuleMapTy::iterator Iter = SPDModuleMap.find(FuncName);
      if (Iter == SPDModuleMap.end()) {
        llvm_unreachable("failed to generate HDL statement");
      }

      emitHDLPrefix(OS, Iter->second);
      emitValue(OS, dyn_cast<Value>(&Instr));
      OS << " = ";
      OS << FuncName;
      OS << "(";
      for (unsigned i = 0; i < (NumOperands - 1); i++) {
        if (i != 0) OS << ", ";
        emitValue(OS, Instr.getOperand(i));
      }
      OS << ")\n";
}
      break;
    case Instruction::Ret:
      if (NumOperands) {
        emitEQUPrefix(OS);
        OS << "__SPD_ret = ";
        emitValue(OS, Instr.getOperand(0));
        OS << ";\n";
      }
      break;
    default:
      llvm_unreachable("unsupported instruction");
    }
  }
}

void emitFunctionSPD(Function &F, SPDModuleMapTy &SPDModuleMap) {
  std::string FuncName = F.getName().str();
  std::error_code EC;
  raw_fd_ostream OS(FuncName + ".spd", EC, sys::fs::F_None);
  if (EC) {
    std::cerr << "cannot create a output file";
  }

  EQUCount = 0;
  HDLCount = 0;
  ValueCount = 0;
  ValueNumMap.clear();

  OS << "// Module " << FuncName << "\n";
  emitFuncDecl(OS, F);

  OS << "\n// equation\n";
  for (BasicBlock &IBB : F) {
    for (BasicBlock::iterator IIB = IBB.begin(), IIE = IBB.end(); IIB != IIE;
         ++IIB) {
      Instruction &Instr = *IIB;
      emitInstruction(OS, Instr, SPDModuleMap);
    }
  }

  if (isEntryFunction(F)) {
    OS << "\n// direct connection\n";
    OS << "DRCT      (Mo::__SPD_sop, Mo::__SPD_eop)";
    OS <<        " = (Mi::__SPD_sop, Mi::__SPD_eop);\n";
  }
}
