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

/*
INITIALIZE_PASS_BEGIN(VirtualTimeLoopIRPass, "vt-loop-ir-pass",
  "Insert loop lookahead function calls", false, false)
INITIALIZE_PASS_END(VirtualTimeLoopIRPass, "vt-loop-ir-pass",
  "Insert loop lookahead function calls", false, false)*/


//static RegisterPass<VirtualTimeLoopIRPass> X("vtlooppass", "Insert loop lookahead function calls");

void VirtualTimeLoopIRPass::analyseLoop(Loop * l, ScalarEvolution &SE, Function * Flookahead) {

    int isFinalIVInvariantInteger = 0;
    int isInitIVInvariantInteger = 0;
    int isStepValueInvariantInteger = 0;
    BasicBlock * currPreHeader;

    currPreHeader = l->getLoopPreheader();
    if (!currPreHeader) {
        outs() << "Loop has no pre-header !" << "\n";
        return;
    }
    Optional<Loop::LoopBounds> Bounds = l->getBounds(SE);

    if (!Bounds.hasValue()) {
        outs() << "SE did not detect any loop-bounds !" << "\n";
        return;
    }

    
    Value * InitialIVValue =&Bounds->getInitialIVValue();
    Value * StepValue = Bounds->getStepValue();
    Value * FinalIVValue = &Bounds->getFinalIVValue();

    /*
    if (Bounds->getStepInst().getOpcode() == Instruction::Sub) {
        outs() << "Step Inst is subtract: " 
            << Bounds->getStepInst().getOpcodeName() << " Num Operands: "
            << Bounds->getStepInst().getNumOperands() << "\n";
        
        Instruction * StepInst = &Bounds->getStepInst();

        if(!SE.getSCEV(StepInst->getOperand(1))) {
            outs() << "SE op1 is null\n";
        }
        if(!SE.getSCEV(StepInst->getOperand(0))) {
            outs() << "SE op0 is null\n";
        }
    }
    else if(Bounds->getStepInst().getOpcode() == Instruction::Add)
        outs() << "Step Inst is add: " << Bounds->getStepInst().getOpcodeName()
            << " Num Operands: " << Bounds->getStepInst().getNumOperands() 
            << "\n";
    */

    if (InitialIVValue && InitialIVValue->getType()->isIntegerTy()) {
        ConstantInt * constInitIVValue =
            dyn_cast_or_null<ConstantInt>(InitialIVValue);
        if (constInitIVValue) {
            outs() << "InitIVValue is a const integer: "
                << (int)constInitIVValue->getZExtValue() << "\n";
        } else {
            outs() << "InitIVValue is an integer \n";
        }

        if (l->isLoopInvariant(InitialIVValue)) {
            outs() << "InitialIVValue is invariant !\n";
            isInitIVInvariantInteger = 1;
        } else {
            outs() << "InitialIVValue is not invariant !\n";
        }
    }


    if (StepValue && StepValue->getType()->isIntegerTy()) {
        ConstantInt * constStepValue =
            dyn_cast_or_null<ConstantInt>(StepValue);
        if (constStepValue) {
            outs() << "StepValue is a const integer: "
                << (int)constStepValue->getZExtValue() << "\n";
        } else {
            outs() << "StepValue is an integer \n";
        }

        if (l->isLoopInvariant(StepValue)) {
            outs() << "StepValue is invariant !\n";
            isStepValueInvariantInteger = 1;
        } else {
            outs() << "StepValue is not invariant !\n";
        }
    } 


    if (FinalIVValue && FinalIVValue->getType()->isIntegerTy()) {
        ConstantInt * constFinalIVValue =
            dyn_cast_or_null<ConstantInt>(FinalIVValue);
        if (constFinalIVValue) {
            outs() << "FinalIVValue is a const integer: "
                << (int)constFinalIVValue->getZExtValue() << "\n";
        } else {
            outs() << "FinalIVValue is an integer \n";
        }

        if (l->isLoopInvariant(FinalIVValue)) {
            outs() << "FinalIVValue is invariant !\n";
            isFinalIVInvariantInteger = 1;
        } else {
            outs() << "FinalIVValue is not invariant !\n";
        }
    }

    if (!isFinalIVInvariantInteger || !isStepValueInvariantInteger
        || !isInitIVInvariantInteger) {
        outs() << "This loop is not suitable for lookahead insertion \n";
        return;
    }

    std::vector<Value *> args;
    args.push_back(InitialIVValue);
    args.push_back(FinalIVValue);
    args.push_back(StepValue);

    //IRBuilder<> builder(Flookahead->getContext());
    //builder.SetInsertPoint(currPreHeader);
    //builder.CreateCall(Flookahead, args, "set_loop_lookahead");

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

bool VirtualTimeLoopIRPass::runOnLoop(Loop * L, LPPassManager &LPM) {


    if (skipLoop(L)) {
	printf("Loop Skipped !\n");
        return false;
    }

   // Get Analysis information.
   LoopInfo &LI = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
   ScalarEvolution &SE = getAnalysis<ScalarEvolutionWrapperPass>().getSE();
   
   BasicBlock * preHeader = L->getLoopPreheader();
   if (!preHeader) {
	outs() << "Loop has no pre-header !\n";
	return false;
   }
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
    	"SetLoopLookahead", M);
	createdExternFunctionDefinition = 1;
   } else {
	Fnew = M->getFunction("SetLoopLookahead");
	assert(Fnew != nullptr);
   }

   outs() << "Holding Function: " << F->getName() << "\n";
   analyseLoop(L, SE, Fnew);
   outs() << "Functions called inside loop: \n";
   outs() << "{\n";
   for (auto *Block : L->getBlocks()) {
     if (LI.getLoopFor(Block) == L) { // Ignore blocks in subloop.
                		       // Incorporate the specified basic block
	for(BasicBlock::iterator i = Block->begin() , ie = Block->end(); i!=ie; ++i){
            if( isa<CallInst>(&(*i)) || isa<InvokeInst>(&(*i))){
                outs() << cast<CallInst>(&(*i))->getCalledFunction()->getName() << "\n";
            }
        }
     }
   }
   outs() << "}\n";
   return true;
} 
