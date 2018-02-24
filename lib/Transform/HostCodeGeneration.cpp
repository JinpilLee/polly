//===- HostCodeGeneration.cpp - generate host-side code for FPGA ----------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass generates a function from each specified loop.
//
//===----------------------------------------------------------------------===//

#include "polly/CodeGen/SPDIR.h"
#include "polly/CodeGen/SPDPrinter.h"
#include "polly/HostCodeGeneration.h"
#include "polly/LinkAllPasses.h"
#include "polly/ScopInfo.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"

using namespace llvm;
using namespace polly;

#define DEBUG_TYPE "polly-host-codegen"

static GlobalVariable *createAllocStreamFunc(SPDStreamInfo *SI,
                                             Module &M, IRBuilder<> &IRB) {
  Type *RetTy = Type::getFloatPtrTy(M.getContext());
  Type *Int64Ty = Type::getInt64Ty(M.getContext());

  Value *Func = M.getOrInsertFunction("__spd_alloc_stream", RetTy, Int64Ty);
  CallInst *RetValue = IRB.CreateCall(Func, {IRB.getInt64(SI->getAllocSize())});

  GlobalVariable *StreamBufferPtr
    = new GlobalVariable(M, RetTy, false, GlobalValue::ExternalLinkage,
                         nullptr, "__spd_stream");

  IRB.CreateStore(RetValue, StreamBufferPtr);

  return StreamBufferPtr;
}

static void createPackFunc(SPDIR &IR, Module &M, IRBuilder<> &IRB,
                           SPDStreamInfo *SI, GlobalVariable *StreamBuffer) {
  Type *RetTy = Type::getVoidTy(M.getContext());
  Type *FloatPtrTy = Type::getFloatPtrTy(M.getContext());
  Type *Int32Ty = Type::getInt32Ty(M.getContext());
  Type *Int64Ty = Type::getInt64Ty(M.getContext());

  Value *Func
    = M.getOrInsertFunction("__spd_pack_contiguous", RetTy,
                            FloatPtrTy, Int32Ty, Int32Ty,
                            FloatPtrTy, Int64Ty);

  for (auto Iter = IR.read_begin(); Iter != IR.read_end(); Iter++) {
    SPDArrayInfo *AI = *Iter;

    SmallVector<Value *, 8> Args;

    Value *SB = IRB.CreateLoad(StreamBuffer);
    Args.push_back(SB);
    Args.push_back(IRB.getInt32(AI->getOffset()));
    Args.push_back(IRB.getInt32(SI->getStride()));

    Value *ArrayRef = AI->getArrayRef();
    ArrayRef = IRB.CreatePointerCast(ArrayRef, FloatPtrTy);
    Args.push_back(ArrayRef);

    uint64_t TotalSize = 1;
    for (uint64_t DimSize : *AI) {
      TotalSize *= DimSize;
    }
    Args.push_back(IRB.getInt64(TotalSize));

    IRB.CreateCall(Func, Args);
  }
}

static void createRunKernelFunc(SPDIR &IR, Module &M, IRBuilder<> &IRB,
                                SPDStreamInfo *RSI, GlobalVariable *RSB,
                                SPDStreamInfo *WSI, GlobalVariable *WSB,
                                uint64_t SwitchInOut) {
  Type *RetTy = Type::getVoidTy(M.getContext());
  Type *FloatPtrPtrTy
    = PointerType::getUnqual(Type::getFloatPtrTy(M.getContext()));
  Type *Int32Ty = Type::getInt32Ty(M.getContext());
  Type *Int64Ty = Type::getInt64Ty(M.getContext());
  
  Value *Func
    = M.getOrInsertFunction("__spd_run_kernel", RetTy,
                            FloatPtrPtrTy, Int64Ty,
                            FloatPtrPtrTy, Int64Ty,
                            Int32Ty);

  SmallVector<Value *, 8> Args;
  Args.push_back(RSB);
  Args.push_back(IRB.getInt64(RSI->getAllocSize()));
  Args.push_back(WSB);
  Args.push_back(IRB.getInt64(WSI->getAllocSize()));
  Args.push_back(IRB.getInt32(SwitchInOut));
  IRB.CreateCall(Func, Args);
}

static void createUnpackFunc(SPDIR &IR, Module &M, IRBuilder<> &IRB,
                             SPDStreamInfo *SI, GlobalVariable *StreamBuffer) {
  Type *RetTy = Type::getVoidTy(M.getContext());
  Type *FloatPtrTy = Type::getFloatPtrTy(M.getContext());
  Type *Int32Ty = Type::getInt32Ty(M.getContext());
  Type *Int64Ty = Type::getInt64Ty(M.getContext());

  Value *Func
    = M.getOrInsertFunction("__spd_unpack_contiguous", RetTy,
                            FloatPtrTy, Int64Ty,
                            FloatPtrTy, Int32Ty, Int32Ty);

  for (auto Iter = IR.write_begin(); Iter != IR.write_end(); Iter++) {
    SPDArrayInfo *AI = *Iter;

    SmallVector<Value *, 8> Args;

    Value *ArrayRef = AI->getArrayRef();
    ArrayRef = IRB.CreatePointerCast(ArrayRef, FloatPtrTy);
    Args.push_back(ArrayRef);

    uint64_t TotalSize = 1;
    for (uint64_t DimSize : *AI) {
      TotalSize *= DimSize;
    }
    Args.push_back(IRB.getInt64(TotalSize));

    Value *SB = IRB.CreateLoad(StreamBuffer);
    Args.push_back(SB);
    Args.push_back(IRB.getInt32(AI->getOffset()));
    Args.push_back(IRB.getInt32(SI->getStride()));

    IRB.CreateCall(Func, Args);
  }
}

static void createFreeStreamFunc(SPDIR &IR, Module &M, IRBuilder<> &IRB,
                                 GlobalVariable *StreamBuffer) {
  Type *RetTy = Type::getVoidTy(M.getContext());
  Type *FloatPtrTy = Type::getFloatPtrTy(M.getContext());

  Value *Func = M.getOrInsertFunction("__spd_free_stream", RetTy, FloatPtrTy);
  Value *SB = IRB.CreateLoad(StreamBuffer);
  IRB.CreateCall(Func, {SB});
}

uint64_t HostCodeGeneration::getRegionNumber(Instruction *Instr) const {
  ConstantInt *RegionInfo = dyn_cast<ConstantInt>(Instr->getOperand(0));
  if (RegionInfo == nullptr) {
    llvm_unreachable("region number should be given as a constant integer");
  }

  return RegionInfo->getZExtValue();
}

const Scop *HostCodeGeneration::getScopFromInstr(Instruction *Instr,
                                                 ScopInfo *SI) const {
  BasicBlock *BB = Instr->getParent();
  for (auto &It : *SI) {
    Region *R = It.first;
    if (R->contains(BB)) {
      return It.second.get();
    }
  }

  llvm_unreachable("cannot find a scop");
}

bool HostCodeGeneration::doInitialization(Module &M) {
  for (Function &F : M) {
    for (BasicBlock &BB : F) {
      for (BasicBlock::iterator Iter = BB.begin(); Iter != BB.end(); ) {
        Instruction *Instr = &*Iter++;
        CallInst *CI = dyn_cast<CallInst>(Instr);
        if (CI == nullptr) continue;

        Function *Func = CI->getCalledFunction();
        if (Func->getName().equals("__spd_begin")) {
          RegionBeginMap[getRegionNumber(CI)] = CI;
        }
        else if (Func->getName().equals("__spd_end")) {
          RegionEndMap[getRegionNumber(CI)] = CI;
        }
      }
    }
  }

  return false;
}

bool HostCodeGeneration::runOnFunction(Function &F) {
  ScopInfo *SI = getAnalysis<ScopInfoWrapperPass>().getSI();

  bool Changed = false;

  if (MDNode *Node = F.getMetadata("polly_extracted_loop")) {
    ValueAsMetadata *VM = dyn_cast<ValueAsMetadata>(Node->getOperand(0));
    ConstantAsMetadata *CM = dyn_cast<ConstantAsMetadata>(Node->getOperand(1));
    uint64_t RegionNumber
      = dyn_cast<ConstantInt>(CM->getValue())->getZExtValue();
    CM = dyn_cast<ConstantAsMetadata>(Node->getOperand(2));
    uint64_t VectorLength
      = dyn_cast<ConstantInt>(CM->getValue())->getZExtValue();
    CM = dyn_cast<ConstantAsMetadata>(Node->getOperand(3));
    uint64_t SwitchInOut
      = dyn_cast<ConstantInt>(CM->getValue())->getZExtValue();

    const Scop *S
      = getScopFromInstr(dyn_cast<Instruction>(VM->getValue()), SI);
    auto &LI = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
    auto &SE = getAnalysis<ScalarEvolutionWrapperPass>().getSE();
    SPDIR IR(*S, LI, SE);
    SPDPrinter Print(&IR, VectorLength);

    // FIXME consider better impl than using counter
    unsigned InstCount = 0;
    for (auto UI = F.use_begin(), UE = F.use_end(); UI != UE;) {
      Use *U = &*UI;
      ++UI;
      ++InstCount;
      CallInst *Caller = dyn_cast<CallInst>(U->getUser());
      assert(Caller != nullptr && "user should be a function call");

      Module *M = F.getParent();

      SPDStreamInfo *RSI = IR.getReadStream();
      SPDStreamInfo *WSI = IR.getWriteStream();

      // region begin
      Instruction *InsertInstr = RegionBeginMap[RegionNumber];
      if (InsertInstr == nullptr) InsertInstr = Caller;
      IRBuilder<> IRB(InsertInstr); 
      GlobalVariable *ReadStreamBuffer
        = createAllocStreamFunc(RSI, *M, IRB);
      GlobalVariable *WriteStreamBuffer
        = createAllocStreamFunc(WSI, *M, IRB);
      createPackFunc(IR, *M, IRB, RSI, ReadStreamBuffer);

      // kernel run
      if (InsertInstr != Caller) IRB.SetInsertPoint(Caller);
      createRunKernelFunc(IR, *M, IRB,
                          RSI, ReadStreamBuffer,
                          WSI, WriteStreamBuffer,
                          SwitchInOut);

      // begion end
      InsertInstr = RegionEndMap[RegionNumber];
      if (InsertInstr != nullptr) IRB.SetInsertPoint(InsertInstr);
      createUnpackFunc(IR, *M, IRB, WSI, WriteStreamBuffer);
      createFreeStreamFunc(IR, *M, IRB, ReadStreamBuffer);
      createFreeStreamFunc(IR, *M, IRB, WriteStreamBuffer);

      Caller->eraseFromParent();

      Changed = true;
    }
    assert(InstCount == 1 && "assuming one caller per extracted func");
  }

  return Changed;
}

void HostCodeGeneration::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<ScopInfoWrapperPass>();
  AU.addRequired<LoopInfoWrapperPass>();
  AU.addRequired<ScalarEvolutionWrapperPass>();

  AU.addPreserved<ScopInfoWrapperPass>();
  AU.addPreserved<LoopInfoWrapperPass>();
  AU.addPreserved<ScalarEvolutionWrapperPass>();
}

char HostCodeGeneration::ID = 0;

Pass *polly::createHostCodeGenerationPass() {
  return new HostCodeGeneration();
}

INITIALIZE_PASS_BEGIN(HostCodeGeneration, "polly-host-codegen",
                      "Polly - Host Code Generation", false, false);
INITIALIZE_PASS_DEPENDENCY(ScopInfoWrapperPass);
INITIALIZE_PASS_DEPENDENCY(LoopInfoWrapperPass);
INITIALIZE_PASS_DEPENDENCY(ScalarEvolutionWrapperPass);
INITIALIZE_PASS_END(HostCodeGeneration, "polly-host-codegen",
                    "Polly - Host Code Generation", false, false)
