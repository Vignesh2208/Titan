
#include "llvm/IR/Argument.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CallSite.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/GlobalVariable.h"
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


namespace llvm {
    class VirtualTimeLoopIRPass : public LoopPass {


        public:
        static char ID; // Pass identification, replacement for typeid
        int createdExternFunctionDefinition;
        int numIgnoredLoops;
        int numProcessedLoops;

        VirtualTimeLoopIRPass() : LoopPass(ID) {
            createdExternFunctionDefinition = 0;
            numIgnoredLoops = 0, numProcessedLoops = 0;
        }

        bool runOnLoop(Loop * L,  LPPassManager &LPM) override;

        StringRef getPassName() const override {
            return "VT Loop Lookahead IR Pass"; }

        void analyseLoop(Loop * l, ScalarEvolution &SE, Function * Flookahead);

        void getAnalysisUsage(AnalysisUsage &AU) const override {
            AU.addRequired<LoopInfoWrapperPass>();
            AU.addRequiredID(LoopSimplifyID);
            AU.addRequired<ScalarEvolutionWrapperPass>();     
        }

        bool doFinalization() override;

  };
}
