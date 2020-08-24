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
DisableVtInsertion("no-cache-la-insertion",
        cl::init(false), cl::Hidden,
        cl::desc("Disable cache simulation insertion"));

char CacheSimPass::ID = 0;

char &llvm::CacheSimPassID = CacheSimPass::ID;


INITIALIZE_PASS(CacheSimPass, "vt-cache-sim-ir-pass",
                "Insert cache simulation function calls", false, false)


bool CacheSimPass::runOnFunction(Function &F) {

    Function * FInsnCacheCallback;
    Function * FDataReadCacheCallback;
    Function * FDataWriteCacheCallback;
    Module * M;
    bool skip = false;
    M = F.getParent();

    const DataLayout& dataLayout = M->getDataLayout();

    

    assert(M != nullptr);

    if (!createdExternFunctionDefinitions) {
            FInsnCacheCallback = Function::Create(
                FunctionType::get(
                    Type::getVoidTy(F.getContext()), false),
                Function::ExternalLinkage,
                INS_CACHE_CALLBACK_FN, M);
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
            createdExternFunctionDefinitions = 1;
    } else {
            FInsnCacheCallback = M->getFunction(INS_CACHE_CALLBACK_FN);
            FDataReadCacheCallback = M->getFunction(
                DATA_READ_CACHE_CALLBACK_FN);
            FDataWriteCacheCallback = M->getFunction(
                DATA_WRITE_CACHE_CALLBACK_FN);
    }

    assert(FInsnCacheCallback != nullptr);
    assert(FDataReadCacheCallback != nullptr);
    assert(FDataWriteCacheCallback != nullptr);

    for (BasicBlock &BB : F) {
        /*Instruction* fi = BB.getFirstNonPHI();
        IRBuilder<> builder(&BB);
        builder.CreateCall(FInsnCacheCallback);*/
    
        BasicBlock *b = &BB;
        for(BasicBlock::iterator BI = b->begin(), BE = b->end(); BI != BE; ++BI) {
            /*if(isa<CallInst>(&(*BI))) {  
            
                StringRef calledFnName = 
                    cast<CallInst>(&(*BI))->getCalledFunction()->getName();
                if (calledFnName == INS_CACHE_CALLBACK_FN){
                    Instruction *newInst = (Instruction *)&(*BI);
                    newInst->moveBefore(fi);
                }
            }*/
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
    return true;
} 

