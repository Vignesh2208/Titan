
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
#include "llvm/CodeGen/Analysis.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/IR/PassManager.h"

namespace llvm {
class CacheSimPass : public FunctionPass {
    public:
    
    static char ID;
    int createdExternFunctionDefinitions;
    CacheSimPass() : FunctionPass(ID) {
        createdExternFunctionDefinitions = 0;
    }
    bool runOnFunction(Function &F) override;

    StringRef getPassName() const override {
        return "VT CacheSim IR Pass";}
}; // end of class CacheSimPass
}  // end of llvm namespace
