#include <vector>
#include <memory>

#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/Support/JSON.h"

#define X86_VIRTUAL_TIME_MANAGER_PASS_NAME "virtual time manager pass"


namespace llvm {
  
  class MachineDominatorTree;
  class MachineLoopInfo;
  class MachineBasicBlock;
  class MachineFunction;

  class VirtualTimeManager : public MachineFunctionPass {

    private:
        void __acquireFlock();
        void __releaseFlock();
        void __insertVtlLogic(MachineFunction &MF, MachineBasicBlock* origMBB,
                              long blockNumber, int bbTimeConsumed,
                              long LoopID);
        void __insertVtStubFn(MachineFunction &MF);

        
    public:
    static char ID;
    
    bool CreatedExternDefinitions;
    long globalBBCounter;
    long globalLoopCounter;
    int lockFd;
    llvm::json::Object perModuleObj;
    
    MachineModuleInfo * MMI;
    MachineLoopInfo * MLI;
    MachineDominatorTree * MDT;

    std::unique_ptr<MachineDominatorTree> OwnedMDT;
    std::unique_ptr<MachineLoopInfo> OwnedMLI;

    VirtualTimeManager() : MachineFunctionPass(ID) {
	      CreatedExternDefinitions = false;
        globalBBCounter = -1;
        lockFd = -1;
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

