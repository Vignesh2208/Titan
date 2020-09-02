#include <vector>
#include <memory>

#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/Support/JSON.h"
#include "VirtualTimeManagementIncludes.h"

#define X86_VIRTUAL_TIME_MANAGER_PASS_NAME "virtual time manager pass"


namespace llvm {
  
  class MachineDominatorTree;
  class MachineLoopInfo;
  class MachineBasicBlock;
  class MachineFunction;
  class TargetMachineSpecificInfo;

  class VirtualTimeManager : public MachineFunctionPass {

    private:

        #ifndef DISABLE_LOOKAHEAD
        void __acquireFlock();
        void __releaseFlock();
        void __insertVtlLogic(MachineFunction &MF, MachineBasicBlock* origMBB,
                              long blockNumber, long LoopID);
        #else
        void __insertVtlLogic(MachineFunction &MF, MachineBasicBlock* origMBB);
        #endif
        void __insertVtStubFn(MachineFunction &MF);

        
    public:
    static char ID;
    bool CreatedExternDefinitions;
    TargetMachineSpecificInfo * targetMachineSpecificInfo = nullptr;

    #ifndef DISABLE_LOOKAHEAD
    long globalBBCounter;
    long globalLoopCounter;
    int lockFd;
    llvm::json::Object perModuleObj;
    MachineLoopInfo * MLI;
    MachineDominatorTree * MDT;
    std::unique_ptr<MachineDominatorTree> OwnedMDT;
    std::unique_ptr<MachineLoopInfo> OwnedMLI;
    #endif

    MachineModuleInfo * MMI;

    VirtualTimeManager() : MachineFunctionPass(ID) {
	      CreatedExternDefinitions = false;

        #ifndef DISABLE_LOOKAHEAD
        globalBBCounter = -1;
        lockFd = -1;
        #endif
    }

    bool runOnMachineFunction(MachineFunction &MF) override;
    void createStubFunction(Module &M);
    void createExternDefinitions(Module &M, bool ContainsMain);
    void printGlobalVar(llvm::GlobalVariable * gVar);

    bool doInitialization(Module &M) override;
    bool doFinalization(Module &M) override;

    StringRef getPassName() const override {
      return X86_VIRTUAL_TIME_MANAGER_PASS_NAME; }
  };
}

