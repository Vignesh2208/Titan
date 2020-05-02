#include <llvm/PassRegistry.h>
#include <llvm/CodeGen/MachineFunctionPass.h>
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/JSON.h"
#include "llvm/CodeGen/MachineJumpTableInfo.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"

#include <sys/file.h>
#include <unistd.h>
#include <fstream>
#include <unordered_map>

#define X86_VIRTUAL_TIME_MANAGER_PASS_NAME "virtual time manager pass"
#define MAIN_FUNC "main"
#define VT_STUB_FUNC "__Vt_Stub"
#define VT_CALLBACK_FUNC "vtCallbackFn"
#define VT_INSN_COUNTER_VAR "currBurstLength"
#define VT_CURR_BBID_VAR "currBBID"
#define VT_CURR_BBL_SIZE_VAR  "currBBSize"
#define VT_FORCE_INVOKE_CALLBACK_VAR "alwaysOn"

namespace llvm {

   class VirtualTimeManager : public MachineFunctionPass {

    private:
        void __acquireFlock();
        void __releaseFlock();
        void __insertVtlLogic(MachineFunction &MF, MachineBasicBlock* origMBB,
                              long blockNumber, int bbTimeConsumed);
        void __insertVtStubFn(MachineFunction &MF);

        llvm::json::Array getAllEdgesFromBBL(long bblNumber,
	        std::unordered_map<long, std::vector<long>>& edges);

        llvm::json::Array getAllCalledFnsFromBBL(long bblNumber,
	        std::unordered_map<long, std::vector<std::string>>& calledFunctions);

        llvm::json::Object composeBBLObject(long bblNumber,
	        std::unordered_map<long, std::vector<std::string>>& calledFunctions,
	        std::unordered_map<long, std::vector<long>>& edges, long bblWeight);
    public:
    static char ID;
    
    bool CreatedExternDefinitions;
    long globalBBCounter;
    MachineModuleInfo *MMI;
    int lockFd;
    llvm::json::Object perModuleObj;

    VirtualTimeManager() : MachineFunctionPass(ID) {
        //initializeVirtualTimeManagerPass(*PassRegistry::getPassRegistry());
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

    StringRef getPassName() const override { return X86_VIRTUAL_TIME_MANAGER_PASS_NAME; }
  };
}
