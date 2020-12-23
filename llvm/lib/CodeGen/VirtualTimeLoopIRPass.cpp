#include "VirtualTimeManagementIncludes.h"
#include "VirtualTimeLoopIRPass.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "iostream"
#include "llvm/Pass.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include <llvm/IR/Instructions.h>
#include <llvm/Analysis/LoopInfo.h>


using namespace llvm;


static cl::opt<bool>
DisableVtInsertion("no-loop-la-insertion",
        cl::init(false), cl::Hidden,
        cl::desc("Disable loop lookahead insertion"));

char VirtualTimeLoopIRPass::ID = 0;

char &llvm::VirtualTimeLoopIRPassID = VirtualTimeLoopIRPass::ID;


INITIALIZE_PASS(VirtualTimeLoopIRPass, "vt-loop-ir-pass",
                "Insert loop lookahead function calls", false, false)

// Logic borrowed from llvm LoopNestAnalysis
bool VirtualTimeLoopIRPass::checkLoopsStructure(Loop &OuterLoop, Loop &InnerLoop) {
   // The inner loop must be the only outer loop's child.
   if ((OuterLoop.getSubLoops().size() != 1) ||
       (InnerLoop.getParentLoop() != &OuterLoop))
     return false;
 
   // We expect loops in normal form which have a preheader, header, latch...
   if (!OuterLoop.isLoopSimplifyForm() || !InnerLoop.isLoopSimplifyForm())
     return false;
 
   const BasicBlock *OuterLoopHeader = OuterLoop.getHeader();
   const BasicBlock *OuterLoopLatch = OuterLoop.getLoopLatch();
   const BasicBlock *InnerLoopPreHeader = InnerLoop.getLoopPreheader();
   const BasicBlock *InnerLoopLatch = InnerLoop.getLoopLatch();
   const BasicBlock *InnerLoopExit = InnerLoop.getExitBlock();
 
   // We expect rotated loops. The inner loop should have a single exit block.
   if (OuterLoop.getExitingBlock() != OuterLoopLatch ||
       InnerLoop.getExitingBlock() != InnerLoopLatch || !InnerLoopExit)
     return false;
 
   // Returns whether the block `ExitBlock` contains at least one LCSSA Phi node.
   auto ContainsLCSSAPhi = [](const BasicBlock &ExitBlock) {
     return any_of(ExitBlock.phis(), [](const PHINode &PN) {
       return PN.getNumIncomingValues() == 1;
     });
   };
 
   // Returns whether the block `BB` qualifies for being an extra Phi block. The
   // extra Phi block is the additional block inserted after the exit block of an
   // "guarded" inner loop which contains "only" Phi nodes corresponding to the
   // LCSSA Phi nodes in the exit block.
   auto IsExtraPhiBlock = [&](const BasicBlock &BB) {
     return BB.getFirstNonPHI() == BB.getTerminator() &&
            all_of(BB.phis(), [&](const PHINode &PN) {
              return all_of(PN.blocks(), [&](const BasicBlock *IncomingBlock) {
                return IncomingBlock == InnerLoopExit ||
                       IncomingBlock == OuterLoopHeader;
              });
            });
   };
 
   const BasicBlock *ExtraPhiBlock = nullptr;
   // Ensure the only branch that may exist between the loops is the inner loop
   // guard.
   if (OuterLoopHeader != InnerLoopPreHeader) {
     const BranchInst *BI =
         dyn_cast<BranchInst>(OuterLoopHeader->getTerminator());
 
     if (!BI || BI != getLoopGuardBranch(&InnerLoop))
       return false;
 
     bool InnerLoopExitContainsLCSSA = ContainsLCSSAPhi(*InnerLoopExit);
 
     // The successors of the inner loop guard should be the inner loop
     // preheader and the outer loop latch.
     for (const BasicBlock *Succ : BI->successors()) {
       if (Succ == InnerLoopPreHeader)
         continue;
       if (Succ == OuterLoopLatch)
         continue;
 
       // If `InnerLoopExit` contains LCSSA Phi instructions, additional block
       // may be inserted before the `OuterLoopLatch` to which `BI` jumps. The
       // loops are still considered perfectly nested if the extra block only
       // contains Phi instructions from InnerLoopExit and OuterLoopHeader.
       if (InnerLoopExitContainsLCSSA && IsExtraPhiBlock(*Succ) &&
           Succ->getSingleSuccessor() == OuterLoopLatch) {
         // Points to the extra block so that we can reference it later in the
         // final check. We can also conclude that the inner loop is
         // guarded and there exists LCSSA Phi node in the exit block later if we
         // see a non-null `ExtraPhiBlock`.
         ExtraPhiBlock = Succ;
         continue;
       }
 
       DEBUG_WITH_TYPE(VerboseDebug, {
         dbgs() << "Inner loop guard successor " << Succ->getName()
                << " doesn't lead to inner loop preheader or "
                   "outer loop latch.\n";
       });
       return false;
     }
   }
 
   // Ensure the inner loop exit block leads to the outer loop latch.
   /*const BasicBlock *SuccInner = InnerLoopExit->getSingleSuccessor();
   if (!SuccInner ||
       (SuccInner != OuterLoopLatch && SuccInner != ExtraPhiBlock)) {
     DEBUG_WITH_TYPE(
         VerboseDebug,
         dbgs() << "Inner loop exit block " << *InnerLoopExit
                << " does not directly lead to the outer loop latch.\n";);
     return false;
   }*/
   return true;
 }


//Logic borrowed from llvm LoopNestAnalysis 
// Only allows nested loops of following form
// for (i = ..; i < ..; i += ..) {
// 	for (j = ..; j < ..; j += ..) {
//		for (k = ..; k < ..; k += ..) {
//			...
//		}
//		postLoopStmnts;
//	}
//	postLoopStmnts;
// }
//  
bool VirtualTimeLoopIRPass::arePerfectlyNested(Loop &OuterLoop, Loop &InnerLoop,
                                   ScalarEvolution &SE) {
   assert(!OuterLoop.isInnermost() && "Outer loop should have subloops");
   assert(!InnerLoop.isOutermost() && "Inner loop should have a parent");
   LLVM_DEBUG(dbgs() << "Checking whether loop '" << OuterLoop.getName()
                     << "' and '" << InnerLoop.getName()
                     << "' are perfectly nested.\n");
 

 
   // Bail out if we cannot retrieve the outer loop bounds.
   auto OuterLoopLB = OuterLoop.getBounds(SE);
   if (OuterLoopLB == None) {
     outs() << "Cannot compute loop bounds of OuterLoop: "
            << OuterLoop << "\n";
     return false;
   }


   // Determine whether the loops structure satisfies the following requirements:
   //  - the inner loop should be the outer loop's only child
   //  - the outer loop header should 'flow' into the inner loop preheader
   //    or jump around the inner loop to the outer loop latch
   if (!checkLoopsStructure(OuterLoop, InnerLoop)) {
     outs() <<  "Not perfectly nested: invalid loop structure.\n";
     return false;
   }
 
   

   
   // Identify the outer loop latch comparison instruction.
   const BasicBlock *Latch = OuterLoop.getLoopLatch();
   assert(Latch && "Expecting a valid loop latch");
   const BranchInst *BI = dyn_cast<BranchInst>(Latch->getTerminator());
   assert(BI && BI->isConditional() &&
          "Expecting loop latch terminator to be a branch instruction");
 
   const CmpInst *OuterLoopLatchCmp = dyn_cast<CmpInst>(BI->getCondition());
   DEBUG_WITH_TYPE(
       VerboseDebug, if (OuterLoopLatchCmp) {
         dbgs() << "Outer loop latch compare instruction: " << *OuterLoopLatchCmp
                << "\n";
       });
 
   // Identify the inner loop guard instruction.
   BranchInst *InnerGuard = getLoopGuardBranch(&InnerLoop);
   const CmpInst *InnerLoopGuardCmp =
       (InnerGuard) ? dyn_cast<CmpInst>(InnerGuard->getCondition()) : nullptr;
 
   DEBUG_WITH_TYPE(
       VerboseDebug, if (InnerLoopGuardCmp) {
         dbgs() << "Inner loop guard compare instruction: " << *InnerLoopGuardCmp
                << "\n";
       });
 
   // Determine whether instructions in a basic block are one of:
   //  - the inner loop guard comparison
   //  - the outer loop latch comparison
   //  - the outer loop induction variable increment
   //  - a phi node, a cast or a branch
   auto containsOnlySafeInstructions = [&](const BasicBlock &BB) {
     return llvm::all_of(BB, [&](const Instruction &I) {
	/* TODO: The following logic only allows very specific instructions in the InnerLoop pre-header.
      		This greatly restricts the type of nested loops allowed. Commenting this out for now.
		The problem is that only very few instructions are safe to speculatively execute. Need
		to think about why this check is generally needed.
   	*/
       /*bool isAllowed = isSafeToSpeculativelyExecute(&I) || isa<PHINode>(I) ||
                        isa<BranchInst>(I);
       if (!isAllowed) {
         DEBUG_WITH_TYPE(VerboseDebug, {
           dbgs() << "Instruction: " << I << "\nin basic block: " << BB
                  << " is considered unsafe.\n";
         });
         return false;
       }*/
 
       // The only binary instruction allowed is the outer loop step instruction,
       // the only comparison instructions allowed are the inner loop guard
       // compare instruction and the outer loop latch compare instruction.
       if ((isa<BinaryOperator>(I) && &I != &OuterLoopLB->getStepInst()) ||
	   (isa<CmpInst>(I) && &I != OuterLoopLatchCmp &&
            &I != InnerLoopGuardCmp)) {
         DEBUG_WITH_TYPE(VerboseDebug, {
           dbgs() << "Instruction: " << I << "\nin basic block:" << BB
                  << "is unsafe.\n";
         });
         return false;
       }
       return true;
     });
   };
 
   // Check the code surrounding the inner loop for instructions that are deemed
   // unsafe.
   const BasicBlock *OuterLoopHeader = OuterLoop.getHeader();
   //const BasicBlock *OuterLoopLatch = OuterLoop.getLoopLatch();
   const BasicBlock *InnerLoopPreHeader = InnerLoop.getLoopPreheader();
 
   if (!containsOnlySafeInstructions(*OuterLoopHeader) ||
       //!containsOnlySafeInstructions(*OuterLoopLatch) ||
       (InnerLoopPreHeader != OuterLoopHeader &&
        !(InnerLoop.getSubLoops().size() == 0) && !containsOnlySafeInstructions(*InnerLoopPreHeader)) ||
       !containsOnlySafeInstructions(*InnerLoop.getExitBlock())) {
     LLVM_DEBUG(dbgs() << "Not perfectly nested: code surrounding inner loop is "
                          "unsafe\n";);
     return false;
   }
 
   LLVM_DEBUG(dbgs() << "Loop '" << OuterLoop.getName() << "' and '"
                     << InnerLoop.getName() << "' are perfectly nested.\n");
   
 
   return true;
}

bool VirtualTimeLoopIRPass::isLoopInteresting(Loop * l, ScalarEvolution &SE) {
    Optional<Loop::LoopBounds> Bounds = l->getBounds(SE);

    if (!Bounds.hasValue()) {
        return false;
    }

    bool isFinalIVInvariantInteger = false;
    bool isInitIVInvariantInteger = false;
    bool isStepValueInvariantInteger = false;
    
    Value * InitialIVValue =&Bounds->getInitialIVValue();
    Value * StepValue = Bounds->getStepValue();
    Value * FinalIVValue = &Bounds->getFinalIVValue();


    if (InitialIVValue && InitialIVValue->getType()->isIntegerTy()) {
        if (l->isLoopInvariant(InitialIVValue)) {
            isInitIVInvariantInteger = true;
        }
    }


    if (StepValue && StepValue->getType()->isIntegerTy()) {
        if (l->isLoopInvariant(StepValue)) {
            isStepValueInvariantInteger = true;
        }
    } 


    if (FinalIVValue && FinalIVValue->getType()->isIntegerTy()) {
        if (l->isLoopInvariant(FinalIVValue)) {
            isFinalIVInvariantInteger = true;
        }
    }

    if (!isFinalIVInvariantInteger ||
        !isStepValueInvariantInteger ||
        !isInitIVInvariantInteger) {
        return false;
    }
    return true;         
}

void VirtualTimeLoopIRPass::analyseLoop(Loop * l, ScalarEvolution &SE, Function * Flookahead) {

    bool isFinalIVInvariantInteger = false;
    bool isInitIVInvariantInteger = false;
    bool isStepValueInvariantInteger = false;
    BasicBlock * currPreHeader;
    int loopDepth = 0;
    int checkIntersting = 0;

    currPreHeader = l->getLoopPreheader();
    Function * F = currPreHeader->getParent();
    assert(currPreHeader);

    Optional<Loop::LoopBounds> Bounds = l->getBounds(SE);

    if (!Bounds.hasValue()) {
        //outs() << "LOOPIR <INFO>: Ignoring loop where SE did not detect any bounds ..." << "\n";
        numIgnoredLoops++;
        return;
    }

    
    Value * InitialIVValue =&Bounds->getInitialIVValue();
    Value * StepValue = Bounds->getStepValue();
    Value * FinalIVValue = &Bounds->getFinalIVValue();


    if (InitialIVValue && InitialIVValue->getType()->isIntegerTy()) {
        if (l->isLoopInvariant(InitialIVValue)) {
            isInitIVInvariantInteger = true;
        }
    }


    if (StepValue && StepValue->getType()->isIntegerTy()) {
        if (l->isLoopInvariant(StepValue)) {
            isStepValueInvariantInteger = true;
        }
    } 


    if (FinalIVValue && FinalIVValue->getType()->isIntegerTy()) {
        if (l->isLoopInvariant(FinalIVValue)) {
            isFinalIVInvariantInteger = true;
        }
    }

    if (!isFinalIVInvariantInteger ||
        !isStepValueInvariantInteger ||
        !isInitIVInvariantInteger) {
        numIgnoredLoops ++;
        return;
    }

    std::vector<Value *> args;
    args.push_back(InitialIVValue);
    args.push_back(FinalIVValue);
    args.push_back(StepValue);

    // Checking if parent loop exists and this loop is eligible for being considered
    // for nested lookahead setting

    Loop * parentLoop = l->getParentLoop();
    Loop * currLoop = l;
    bool isCandidateNested = true;
    if (!parentLoop)
	    isCandidateNested = false;
    
    while (parentLoop != NULL && isCandidateNested) {
         
        if (!isLoopInteresting(parentLoop, SE)) {
	          outs() << "Parent Loop is Not interesting !\n";
            // Make sure no other parent/ancestor loop should be interesting
            checkIntersting = 1;
        } else if (checkIntersting) {
          // We don't deal with some intermediate loop being non-interesting
          // If some ancestor is not interesting, then every ancestor from that
          // point should be not interesting all the way up-to the top
          isCandidateNested = false;
          break;
        }

	      if (!checkIntersting && !parentLoop->isLoopInvariant(InitialIVValue)) {
	          outs() << "Init Value is Not Invariant in Parent !\n";
            isCandidateNested = false;
            break;
        }

        if (!checkIntersting && !parentLoop->isLoopInvariant(FinalIVValue)) {
	          outs() << "Final Value is Not Invariant in Parent !\n";
            isCandidateNested = false;
            break;
        }

        if (!checkIntersting && !parentLoop->isLoopInvariant(StepValue)) {
	          outs() << "Step Value is Not Invariant in Parent !\n";
            isCandidateNested = false;
            break;
        }

        if (!checkIntersting && !arePerfectlyNested(*parentLoop, *currLoop, SE)) {
            isCandidateNested = false;
            outs() << "Loop Structure did not fit between parent and child loops !\n";
            break;
        }
        currLoop = parentLoop;     
        parentLoop = parentLoop->getParentLoop();

        if (!checkIntersting)
          loopDepth++;
    }
    if (!isCandidateNested && !l->getParentLoop()) // no parent loop
	    loopDepth = 0;
    else if (!isCandidateNested){
	    // something went wrong going up the list of parents
	    loopDepth = -1;
    }

    outs() << "Processing Loop with depth: " << loopDepth << "\n";
    auto loopDepthValue = llvm::ConstantInt::get(F->getContext(), llvm::APInt(32, loopDepth, true));
    args.push_back(loopDepthValue);

    int count = 0;
    int max = currPreHeader->size();
    for(BasicBlock::iterator i = currPreHeader->begin() ,
        ie = currPreHeader->end(); i!=ie; ++i){
        Instruction * inst = &*i;
        if (count == max - 1) {
            IRBuilder<> builder(inst);
            builder.CreateCall(Flookahead, args);
        }
        count ++;
    }


}

bool VirtualTimeLoopIRPass::doFinalization() {
    #ifndef DISABLE_LOOKAHEAD
    outs() << "LOOPIR<INFO> Number of ProcessedLoops: " << numProcessedLoops
           << " Number of IgnoredLoops: " << numIgnoredLoops << "\n";
    #endif
    return false; 
}

bool VirtualTimeLoopIRPass::runOnLoop(Loop * L, LPPassManager &LPM) {


    #ifdef DISABLE_LOOKAHEAD
    return false;
    #else

    if (skipLoop(L)) {
        //outs() << "LOOPIR <INFO>: Skipping loop due to optimization flags ...\n";
        numIgnoredLoops ++;
        return false;
    }

    DT = &getAnalysis<DominatorTreeWrapperPass>().getDomTree();
   

    // only consider loops with a single exiting and exit block
    if (!L->getExitingBlock() || !L->getUniqueExitBlock()) {
        numIgnoredLoops ++;
        return false;
    }

   BasicBlock * preHeader = L->getLoopPreheader();
   if (!preHeader) {
        //outs() << "LOOPIR <INFO>: Ignoring loop with no pre-header ...\n";
        numIgnoredLoops ++;
        return false;
   }

   // Get Analysis information.
   ScalarEvolution &SE = getAnalysis<ScalarEvolutionWrapperPass>().getSE();
   
   Function * F = preHeader->getParent();
   assert(F != nullptr);
   Module * M = preHeader->getModule();
   assert(M != nullptr);
   Function *Fnew;

   if (!createdExternFunctionDefinition) {
        std::vector<Type*> Integers(4, Type::getInt32Ty(F->getContext()));
        FunctionType *FT = FunctionType::get(
                Type::getVoidTy(F->getContext()),
                    Integers, false);
        Fnew = Function::Create(FT, Function::ExternalLinkage,
            VT_LOOP_LOOKAHEAD_FUNC, M);
        createdExternFunctionDefinition = 1;
   } else {
        Fnew = M->getFunction(VT_LOOP_LOOKAHEAD_FUNC);
        assert(Fnew != nullptr);
        assert(FNestedNew != nullptr);
   }

   analyseLoop(L, SE, Fnew);
   numProcessedLoops ++;
   return true;
   #endif
} 
