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

LoopPass *llvm::createVirtualTimeLoopIRPass() {
  return new VirtualTimeLoopIRPass();
}

void VirtualTimeLoopIRPass::analyseLoop(Loop * l, ScalarEvolution &SE,
                                        Function * Flookahead) {

    bool isFinalIVInvariantInteger = false;
    bool isInitIVInvariantInteger = false;
    bool isStepValueInvariantInteger = false;
    BasicBlock * currPreHeader;

    currPreHeader = l->getLoopPreheader();
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
        //outs() << "LOOPIR <INFO>: Ignoring unsuitable loop for lookahead insertion ...\n";
        numIgnoredLoops ++;
        return;
    }

    std::vector<Value *> args;
    args.push_back(InitialIVValue);
    args.push_back(FinalIVValue);
    args.push_back(StepValue);

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
    outs() << "LOOPIR<INFO> Number of ProcessedLoops: " << numProcessedLoops
           << " Number of IgnoredLoops: " << numIgnoredLoops << "\n";
    return false; 
}

bool VirtualTimeLoopIRPass::runOnLoop(Loop * L, LPPassManager &LPM) {


    if (skipLoop(L)) {
        //outs() << "LOOPIR <INFO>: Skipping loop due to optimization flags ...\n";
        numIgnoredLoops ++;
        return false;
    }

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
   LoopInfo &LI = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
   ScalarEvolution &SE = getAnalysis<ScalarEvolutionWrapperPass>().getSE();
   
   Function * F = preHeader->getParent();
   assert(F != nullptr);
   Module * M = preHeader->getModule();
   assert(M != nullptr);
   Function *Fnew;

   if (!createdExternFunctionDefinition) {
        std::vector<Type*> Integers(3, Type::getInt32Ty(F->getContext()));
        FunctionType *FT = FunctionType::get(
                Type::getVoidTy(F->getContext()),
                    Integers, false);
        Fnew = Function::Create(FT, Function::ExternalLinkage,
            VT_LOOP_LOOKAHEAD_FUNC, M);
        createdExternFunctionDefinition = 1;
   } else {
        Fnew = M->getFunction(VT_LOOP_LOOKAHEAD_FUNC);
        assert(Fnew != nullptr);
   }

   analyseLoop(L, SE, Fnew);
   numProcessedLoops ++;
//    outs() << "LOOPIR<INFO> Processing loop ...\n";
//    outs() << "{\n";
//    for (auto *Block : L->getBlocks()) {
//      if (LI.getLoopFor(Block) == L) { // Ignore blocks in subloop.
//                 		       // Incorporate the specified basic block
// 	for(BasicBlock::iterator i = Block->begin() , ie = Block->end(); i!=ie; ++i){
//             if( isa<CallInst>(&(*i)) || isa<InvokeInst>(&(*i))){
//                 outs() << cast<CallInst>(&(*i))->getCalledFunction()->getName() << "\n";
//             }
//         }
//      }
//    }
//    outs() << "}\n";
   return true;
} 
