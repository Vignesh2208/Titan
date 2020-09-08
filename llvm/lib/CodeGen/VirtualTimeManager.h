#include <vector>
#include <memory>

#include "llvm/CodeGen/MachineFunctionPass.h"
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
        void __acquireFlock(std::string lockFilePath,
          std::string clangParamsFilePath);
        void __releaseFlock();
        void __insertVtlLogic(MachineFunction &MF, MachineBasicBlock* origMBB,
                              long blockNumber, long LoopID);
        #else
        void __insertVtlLogic(MachineFunction &MF, MachineBasicBlock* origMBB);
        #endif
        void __insertVtStubFn(MachineFunction &MF);

        
    public:
    static char ID;
    std::string projectArchName;
    std::string projectArchTimingsPath;
    bool CreatedExternDefinitions;
    TargetMachineSpecificInfo * targetMachineSpecificInfo = nullptr;

    #ifndef DISABLE_LOOKAHEAD
    long globalBBCounter;
    long globalLoopCounter;
    int lockFd;
    std::string clangLockFilePath;
	  std::string clangInitParamsPath;
    llvm::json::Object perModuleObj;
    MachineLoopInfo * MLI;
    MachineDominatorTree * MDT;
    std::unique_ptr<MachineDominatorTree> OwnedMDT;
    std::unique_ptr<MachineLoopInfo> OwnedMLI;
    #endif

    MachineModuleInfo * MMI;

    VirtualTimeManager() : MachineFunctionPass(ID) {
	      CreatedExternDefinitions = false;
        projectArchName = DEFAULT_PROJECT_ARCH_NAME;
        projectArchTimingsPath = DEFAULT_PROJECT_ARCH_NAME;
        #ifndef DISABLE_LOOKAHEAD
        clangLockFilePath = CLANG_FILE_LOCK;
	      clangInitParamsPath = CLANG_INIT_PARAMS;
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

