
#include "llvm/IR/Argument.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CallSite.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Use.h"
#include "llvm/IR/User.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/ValueHandle.h"
#include "llvm/IR/ValueMap.h"
#include "llvm/Pass.h"
#include <llvm/PassRegistry.h>
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Transforms/Utils.h"
#include "llvm/Analysis/LoopInfo.h"
 #include "llvm/Analysis/LoopPass.h"
#include "llvm/CodeGen/Analysis.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Transforms/Scalar/LoopPassManager.h"
#include "llvm/Analysis/LoopAnalysisManager.h"
#include "llvm/Analysis/ValueTracking.h"
#include <unordered_map>


namespace llvm {
    class VirtualTimeLoopIRPass : public LoopPass {


        public:
        static char ID; // Pass identification, replacement for typeid
        int createdExternFunctionDefinition;
        int numIgnoredLoops;
        int numProcessedLoops;
        DominatorTree *DT;
        
        VirtualTimeLoopIRPass() : LoopPass(ID) {
            createdExternFunctionDefinition = 0;
            numIgnoredLoops = 0, numProcessedLoops = 0;
        }

        bool runOnLoop(Loop * L,  LPPassManager &LPM) override;

	bool isRotatedForm(Loop * L) const {
	     BasicBlock *Latch = L->getLoopLatch();
	     return Latch && L->isLoopExiting(Latch);
	}

        bool isLoopInteresting(Loop * l, ScalarEvolution &SE);
        bool checkLoopsStructure(Loop &OuterLoop, Loop &InnerLoop);

	BranchInst * getLoopGuardBranch(Loop * L) const {
	   if (!L->isLoopSimplifyForm())
	     return nullptr;
	 
	   BasicBlock *Preheader = L->getLoopPreheader();
	   assert(Preheader && L->getLoopLatch() &&
		  "Expecting a loop with valid preheader and latch");
	 
	   // Loop should be in rotate form.
	   if (!isRotatedForm(L))
	     return nullptr;
	 
	   // Disallow loops with more than one unique exit block, as we do not verify
	   // that GuardOtherSucc post dominates all exit blocks.
	   BasicBlock *ExitFromLatch = L->getUniqueExitBlock();
	   if (!ExitFromLatch)
	     return nullptr;
	 
	   BasicBlock *ExitFromLatchSucc = ExitFromLatch->getUniqueSuccessor();
	   if (!ExitFromLatchSucc)
	     return nullptr;
	 
	   BasicBlock *GuardBB = Preheader->getUniquePredecessor();
	   if (!GuardBB)
	     return nullptr;
	 
	   assert(GuardBB->getTerminator() && "Expecting valid guard terminator");
	 
	   BranchInst *GuardBI = dyn_cast<BranchInst>(GuardBB->getTerminator());
	   if (!GuardBI || GuardBI->isUnconditional())
	     return nullptr;
	 
	   BasicBlock *GuardOtherSucc = (GuardBI->getSuccessor(0) == Preheader)
		                            ? GuardBI->getSuccessor(1)
		                            : GuardBI->getSuccessor(0);
	   return (GuardOtherSucc == ExitFromLatchSucc) ? GuardBI : nullptr;
 	}

	bool arePerfectlyNested(Loop &OuterLoop, Loop &InnerLoop, ScalarEvolution &SE);

        StringRef getPassName() const override {
            return "VT Loop Lookahead IR Pass"; }

        void analyseLoop(Loop * l, ScalarEvolution &SE, Function * Flookahead);

        void getAnalysisUsage(AnalysisUsage &AU) const override {
            AU.addRequired<DominatorTreeWrapperPass>();
            AU.addRequired<LoopInfoWrapperPass>();
            AU.addRequiredID(LoopSimplifyID);
            AU.addRequired<ScalarEvolutionWrapperPass>();     
        }

        bool doFinalization() override;

  };
}
