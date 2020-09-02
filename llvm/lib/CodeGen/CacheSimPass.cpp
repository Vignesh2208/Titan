#include "VirtualTimeManagementIncludes.h"
#include "CacheSimPass.h"
#include "llvm/Support/raw_ostream.h"
#include "iostream"
#include "llvm/Pass.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include <llvm/IR/Instructions.h>


using namespace llvm;


static cl::opt<bool>
DisableCacheSimInsertion("no-cache-la-insertion",
        cl::init(false), cl::Hidden,
        cl::desc("Disable cache simulation insertion"));

char CacheSimPass::ID = 0;

char &llvm::CacheSimPassID = CacheSimPass::ID;


INITIALIZE_PASS(CacheSimPass, "vt-cache-sim-ir-pass",
                "Insert cache simulation function calls", false, false)


bool CacheSimPass::runOnFunction(Function &F) {

    #ifndef DISABLE_INSN_CACHE_SIM
    Function * FInsnCacheCallback;
    #endif

    #ifndef DISABLE_DATA_CACHE_SIM
    Function * FDataReadCacheCallback;
    Function * FDataWriteCacheCallback;
    #endif

    if (DisableCacheSimInsertion)
        return false;

    Module * M;
    M = F.getParent();

    #ifndef DISABLE_DATA_CACHE_SIM
    const DataLayout& dataLayout = M->getDataLayout();
    #endif
    
    assert(M != nullptr);

    if (!createdExternFunctionDefinitions) {

        #ifndef DISABLE_INSN_CACHE_SIM
        FInsnCacheCallback = Function::Create(
            FunctionType::get(
                Type::getVoidTy(F.getContext()), false),
            Function::ExternalLinkage,
            INS_CACHE_CALLBACK_FN, M);
        #endif

        #ifndef DISABLE_DATA_CACHE_SIM
        std::vector<Type*> Integers(1, Type::getInt64Ty(F.getContext()));
        Integers.push_back(Type::getInt32Ty(F.getContext()));
        FDataReadCacheCallback = Function::Create(
            FunctionType::get(
                Type::getVoidTy(F.getContext()), Integers, false),
            Function::ExternalLinkage,
            DATA_READ_CACHE_CALLBACK_FN, M);
        FDataWriteCacheCallback = Function::Create(
            FunctionType::get(
                Type::getVoidTy(F.getContext()), Integers, false),
            Function::ExternalLinkage,
            DATA_WRITE_CACHE_CALLBACK_FN, M);
        #endif
        createdExternFunctionDefinitions = 1;
    } else {
        #ifndef DISABLE_INSN_CACHE_SIM
        FInsnCacheCallback = M->getFunction(INS_CACHE_CALLBACK_FN);
        #endif

        #ifndef DISABLE_DATA_CACHE_SIM
        FDataReadCacheCallback = M->getFunction(
            DATA_READ_CACHE_CALLBACK_FN);
        FDataWriteCacheCallback = M->getFunction(
            DATA_WRITE_CACHE_CALLBACK_FN);
        #endif
    }

    #ifndef DISABLE_INSN_CACHE_SIM
    assert(FInsnCacheCallback != nullptr);
    #endif

    #ifndef DISABLE_DATA_CACHE_SIM
    assert(FDataReadCacheCallback != nullptr);
    assert(FDataWriteCacheCallback != nullptr);
    
    for (BasicBlock &BB : F) {
        BasicBlock *b = &BB;
        for(BasicBlock::iterator BI = b->begin(), BE = b->end(); BI != BE; ++BI) {
            Instruction *I = &(*BI);
            if (LoadInst *li = dyn_cast<LoadInst>(I)) {
                Value* liop = &*(li->getPointerOperand());
                std::vector<Value *> args;
                args.push_back(liop);
                PointerType* pointerType = cast<PointerType>(liop->getType());                         
                uint64_t storeSize = dataLayout.getTypeStoreSize(
                    pointerType->getPointerElementType());
                Value * liopSize = llvm::ConstantInt::get(
                    llvm::Type::getInt64Ty(F.getContext()), storeSize);
                args.push_back(liopSize);
                IRBuilder<> builder(I);
                builder.CreateCall(FDataReadCacheCallback, args);

            } else if (StoreInst *si = dyn_cast<StoreInst>(I)) {
                Value* siop = &*(si->getPointerOperand());
                std::vector<Value *> args;
                args.push_back(siop);
                PointerType* pointerType = cast<PointerType>(siop->getType());                         
                uint64_t storeSize = dataLayout.getTypeStoreSize(
                    pointerType->getPointerElementType());
                Value * siopSize = llvm::ConstantInt::get(
                    llvm::Type::getInt64Ty(F.getContext()), storeSize);
                args.push_back(siopSize);
                IRBuilder<> builder(I);
                builder.CreateCall(FDataWriteCacheCallback, args);
            }
        }

    }
    #endif

    #ifndef DISABLE_INSN_CACHE_SIM
    for (BasicBlock &BB : F) {
        Instruction* fi = BB.getFirstNonPHI();
        IRBuilder<> builder(&BB);
        builder.CreateCall(FInsnCacheCallback);
    
        BasicBlock *b = &BB;
        for(BasicBlock::iterator BI = b->begin(), BE = b->end(); BI != BE; ++BI) {
            if(isa<CallInst>(&(*BI))) {  
            
                StringRef calledFnName = 
                    cast<CallInst>(&(*BI))->getCalledFunction()->getName();
                if (calledFnName == INS_CACHE_CALLBACK_FN){
                    Instruction *newInst = (Instruction *)&(*BI);
                    newInst->moveBefore(fi);
                    break;
                }
            }
        }

    }
    #endif

    #if defined(DISABLE_INSN_CACHE_SIM) && defined(DISABLE_DATA_CACHE_SIM)
    return false;
    #else
    return true;
    #endif
} 

