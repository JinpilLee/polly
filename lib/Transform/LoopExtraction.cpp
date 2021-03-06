//===- LoopExtraction.cpp - generate function from loop -------------------===//
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

#include "polly/LinkAllPasses.h"
#include "polly/LoopExtraction.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Metadata.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Utils/CodeExtractor.h"

using namespace llvm;
using namespace polly;

#define DEBUG_TYPE "polly-loop-ext"

bool LoopExtraction::runOnLoop(Loop *L, LPPassManager &) {
  if (skipLoop(L))
    return false;

  // FIXME temporary impl
  // implement directive
  BasicBlock *Header = L->getHeader();
  Instruction *FirstNonPHI = Header->getFirstNonPHI();
  uint64_t RegionNumber = 0;
  uint64_t VectorLength = 1;
  uint64_t UnrollCount = 1;
  uint64_t SwitchInOut = 0;
  CallInst *CI = dyn_cast<CallInst>(FirstNonPHI);
  if (CI == nullptr) {
    return false;
  }
  else {
    if (CI->getCalledFunction()->getName().equals("__spd_loop")) {
      ConstantInt *Op = dyn_cast<ConstantInt>(CI->getOperand(0));
      if (Op == nullptr) {
        llvm_unreachable("region number is not a constant integer");
      }

      RegionNumber = Op->getZExtValue();

      Op = dyn_cast<ConstantInt>(CI->getOperand(1));
      if (Op == nullptr) {
        llvm_unreachable("vector length is not a constant integer");
      }

      VectorLength = Op->getZExtValue();

      Op = dyn_cast<ConstantInt>(CI->getOperand(2));
      if (Op == nullptr) {
        llvm_unreachable("unroll count is not a constant integer");
      }

      UnrollCount = Op->getZExtValue();

      Op = dyn_cast<ConstantInt>(CI->getOperand(3));
      if (Op == nullptr) {
        llvm_unreachable("switch in/out is not a constant integer");
      }
      else if (!(Op->isOne() || Op->isZero())) {
        llvm_unreachable("switch in/out should be a boolean value");
      }

      SwitchInOut = Op->getZExtValue();

      CI->eraseFromParent();
    }
    else {
      return false;
    }
  }

  // If LoopSimplify form is not available, stay out of trouble.
  if (!L->isLoopSimplifyForm())
    return false;

  DominatorTree &DT = getAnalysis<DominatorTreeWrapperPass>().getDomTree();
  LoopInfo &LI = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
  bool Changed = false;

  // If there is more than one top-level loop in this function, extract all of
  // the loops. Otherwise there is exactly one top-level loop; in this case if
  // this function is more than a minimal wrapper around the loop, extract
  // the loop.
  bool ShouldExtractLoop = false;

  // Extract the loop if the entry block doesn't branch to the loop header.
  TerminatorInst *EntryTI =
    L->getHeader()->getParent()->getEntryBlock().getTerminator();
  if (!isa<BranchInst>(EntryTI) ||
      !cast<BranchInst>(EntryTI)->isUnconditional() ||
      EntryTI->getSuccessor(0) != L->getHeader()) {
    ShouldExtractLoop = true;
  } else {
    // Check to see if any exits from the loop are more than just return
    // blocks.
    SmallVector<BasicBlock*, 8> ExitBlocks;
    L->getExitBlocks(ExitBlocks);
    for (unsigned i = 0, e = ExitBlocks.size(); i != e; ++i)
      if (!isa<ReturnInst>(ExitBlocks[i]->getTerminator())) {
        ShouldExtractLoop = true;
        break;
      }
  }

  if (ShouldExtractLoop) {
    // We must omit EH pads. EH pads must accompany the invoke
    // instruction. But this would result in a loop in the extracted
    // function. An infinite cycle occurs when it tries to extract that loop as
    // well.
    SmallVector<BasicBlock*, 8> ExitBlocks;
    L->getExitBlocks(ExitBlocks);
    for (unsigned i = 0, e = ExitBlocks.size(); i != e; ++i)
      if (ExitBlocks[i]->isEHPad()) {
        ShouldExtractLoop = false;
        break;
      }
  }

  if (ShouldExtractLoop) {
    if (NumLoops == 0) return Changed;
    --NumLoops;
    CodeExtractor Extractor(DT, *L);
    Function *ExtractedFunc = Extractor.extractCodeRegion();
    if (ExtractedFunc != nullptr) {
      Changed = true;
      // After extraction, the loop is replaced by a function call, so
      // we shouldn't try to run any more loop passes on it.
      LI.markAsRemoved(L);
      Type *Int64Ty = Type::getInt64Ty(ExtractedFunc->getContext());
      Metadata *MDArgs[] = {
        ValueAsMetadata::get(&*(L->getHeader()->begin())),
        ConstantAsMetadata::get(ConstantInt::get(Int64Ty, RegionNumber)),
        ConstantAsMetadata::get(ConstantInt::get(Int64Ty, VectorLength)),
        ConstantAsMetadata::get(ConstantInt::get(Int64Ty, UnrollCount)),
        ConstantAsMetadata::get(ConstantInt::get(Int64Ty, SwitchInOut))
      };
      ExtractedFunc->setMetadata("polly_extracted_loop",
                                 MDNode::get(ExtractedFunc->getContext(),
                                             MDArgs));
    }
  }

  return Changed;
}

void LoopExtraction::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequiredID(BreakCriticalEdgesID);
  AU.addRequiredID(LoopSimplifyID);
  AU.addRequired<DominatorTreeWrapperPass>();
  AU.addRequired<LoopInfoWrapperPass>();
}

char LoopExtraction::ID = 0;

Pass *polly::createLoopExtractionPass() {
  return new LoopExtraction();
}

INITIALIZE_PASS_BEGIN(LoopExtraction, "polly-loop-ext",
                      "Polly - Loop Extraction", false, false);
INITIALIZE_PASS_DEPENDENCY(BreakCriticalEdges);
INITIALIZE_PASS_DEPENDENCY(LoopSimplify);
INITIALIZE_PASS_DEPENDENCY(DominatorTreeWrapperPass);
INITIALIZE_PASS_DEPENDENCY(LoopInfoWrapperPass);
INITIALIZE_PASS_END(LoopExtraction, "polly-loop-ext",
                    "Polly - Loop Extraction", false, false)
