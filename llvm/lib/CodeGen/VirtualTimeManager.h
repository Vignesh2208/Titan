#include <llvm/PassRegistry.h>
#include <llvm/CodeGen/MachineFunctionPass.h>
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/CodeGen/MachineJumpTableInfo.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"

#define X86_VIRTUAL_TIME_MANAGER_PASS_NAME "virtual time manager pass"

namespace llvm {

   class VirtualTimeManager : public MachineFunctionPass {
    public:
    static char ID;
    
    bool CreatedExternDefinitions;
    long globalBBCounter;
    llvm::StringRef stubFunctionName;
    llvm::StringRef vtControlFunctionName;
    llvm::StringRef vtInsnCounterGlobalVar;
    llvm::StringRef vtCurrBasicBlockIDVar;
    llvm::StringRef vtCurrBBSizeVar;
    llvm::StringRef enabled;
    MachineModuleInfo *MMI;
    bool Printed;

    VirtualTimeManager() : MachineFunctionPass(ID) {
        //initializeVirtualTimeManagerPass(*PassRegistry::getPassRegistry());
        stubFunctionName = "__Vt_Stub";
	vtControlFunctionName = "vtCallbackFn";
	vtInsnCounterGlobalVar = "currBurstLength";
        vtCurrBBSizeVar = "currBBSize";
	vtCurrBasicBlockIDVar = "currBBID";
	enabled = "alwaysOn";
	CreatedExternDefinitions = false;
        Printed = false;
        globalBBCounter = 0;
    }

    bool runOnMachineFunction(MachineFunction &MF) override;
    void createStubFunction(Module &M);
    void createExternDefinitions(Module &M, bool ContainsMain);
    void printGlobalVar(llvm::GlobalVariable * gVar);

    StringRef getPassName() const override { return X86_VIRTUAL_TIME_MANAGER_PASS_NAME; }
  };
}
