#include "VirtualTimeManagementIncludes.h"
#include "VirtualTimeManagementUtils.h"
#include "VirtualTimeManager.h"

#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>




#define GET_INSTRINFO_ENUM
#define GET_REGINFO_ENUM
#include "../Target/X86/X86.h"
#include "../Target/X86/X86GenInstrInfo.inc"
#include "../Target/X86/X86GenRegisterInfo.inc"

#define MO_GOTPCREL 5

using namespace llvm;

static cl::opt<bool>
DisableVtInsertion("no-vt-insertion",
        cl::init(false), cl::Hidden,
        cl::desc("Disable virtual time specific instructions"));

static cl::opt<bool>
IgnoreVtCache("ignore-vt-cache",
        cl::init(true), cl::Hidden,
        cl::desc("Ignores the presence of any existing initialization params/lock file"));

char VirtualTimeManager::ID = 0;

char &llvm::VirtualTimeManagerID = VirtualTimeManager::ID;


INITIALIZE_PASS_BEGIN(VirtualTimeManager, "vt-manager",
  "Insert virtual time counting instructions", false, false)
INITIALIZE_PASS_DEPENDENCY(PEI)
INITIALIZE_PASS_END(VirtualTimeManager, "vt-manager",
  "Insert virtual time counting instructions", false, false)


using namespace llvm;


void VirtualTimeManager::createStubFunction(Module &M) {
   
   LLVMContext &Ctx = M.getContext();
   auto Type = FunctionType::get(Type::getVoidTy(Ctx), false);
   Function *F =
       Function::Create(Type, GlobalValue::LinkOnceODRLinkage,
                           VT_STUB_FUNC, &M);
   F->setVisibility(GlobalValue::DefaultVisibility);
   F->setComdat(M.getOrInsertComdat(VT_STUB_FUNC));
 
   // Add Attributes so that we don't create a frame, unwind information, or
   // inline.
   AttrBuilder B;
   B.addAttribute(llvm::Attribute::NoUnwind);
   B.addAttribute(llvm::Attribute::Naked);
   F->addAttributes(llvm::AttributeList::FunctionIndex, B);
 
   // Populate our function a bit so that we can verify.
   BasicBlock *Entry = BasicBlock::Create(Ctx, "entry", F);
   IRBuilder<> Builder(Entry);
 
   Builder.CreateRetVoid();
 
   // MachineFunctions/MachineBasicBlocks aren't created automatically for the
   // IR-level constructs we already made. Create them and insert them into the
   // module.
   MachineFunction &MF = MMI->getOrCreateMachineFunction(*F);
   MachineBasicBlock *EntryMBB = MF.CreateMachineBasicBlock(Entry);
 
   // Insert EntryMBB into MF. It's not in the module until we do this.
   MF.insert(MF.end(), EntryMBB);
 }

void VirtualTimeManager::printGlobalVar(llvm::GlobalVariable * gvar) {
    
    if (gvar) {
            if (gvar->hasInitializer()) {
            outs() << "Has Initializer \n";
        }
        if (gvar->hasComdat()) {
            outs() << "Has Comdat \n";
        }
        if (gvar->hasMetadata()) {
            outs() << "Has Metadata \n";
        }
        if (gvar->isExternallyInitialized()) {
            outs() << "isExternallyInitialized \n";
        }
        outs() << "Alignment: " << gvar->getAlignment() << "\n";
        if (gvar->hasDefaultVisibility()) {
            outs() << "hasDefaultVisibility \n";
        }
        if (gvar->hasHiddenVisibility()) {
            outs() << "hasHiddenVisibility \n";
        }	
        if (gvar->hasProtectedVisibility() ) {
            outs() << "hasProtectedVisibility \n";
        }
        if (gvar->isInterposable() ) {
            outs() << "isInterposable \n";
        }
        if (gvar->hasExternalLinkage() ) {
            outs() << "hasExternalLinkage \n";
        }

        if (gvar->hasAvailableExternallyLinkage()) {
            outs() << "hasAvailableExternallyLinkage \n";
        }
        if (gvar->hasLinkOnceLinkage() ) {
            outs() << "hasLinkOnceLinkage \n";
        }
        if (gvar->hasLinkOnceODRLinkage() ) {
            outs() << "hasLinkOnceODRLinkage \n";
        }
        if (gvar->hasWeakLinkage() ) {
            outs() << "hasWeakLinkage \n";
        }
        if (gvar->hasWeakAnyLinkage() ) {
            outs() << "hasWeakAnyLinkage \n";
        }
        if (gvar->hasWeakODRLinkage() ) {
            outs() << "hasWeakODRLinkage \n";
        }
        if (gvar->hasAppendingLinkage() ) {
            outs() << "hasAppendingLinkage \n";
        }
        if (gvar->hasInternalLinkage() ) {
            outs() << "hasInternalLinkage \n";
        }
        if (gvar->hasPrivateLinkage()) {
            outs() << "hasPrivateLinkage \n";
        }
        if (gvar->hasLocalLinkage()) {
            outs() << "hasLocalLinkage \n";
        }
        if (gvar->hasExternalWeakLinkage()) {
            outs() << "hasExternalWeakLinkage \n";
        }
        if (gvar->hasCommonLinkage()) {
            outs() << "hasCommonLinkage \n";
        }
        if (gvar->hasValidDeclarationLinkage()) {
            outs() << "hasValidDeclarationLinkage \n";
        }
        gvar->print(outs());
        outs () << "\n";
    }

}


void VirtualTimeManager::createExternDefinitions(Module &M, bool ContainsMain) {
   LLVMContext &Ctx = M.getContext();

  
   // VT_CYCLES_COUNTER_VAR
    M.getOrInsertGlobal(VT_CYCLES_COUNTER_VAR, Type::getInt64Ty(Ctx));
    auto *gVar = M.getNamedGlobal(VT_CYCLES_COUNTER_VAR);
    gVar->setLinkage(GlobalValue::ExternalLinkage);
    gVar->setAlignment(8);

    #ifndef DISABLE_INSN_CACHE_SIM
    // VT_CURR_BBL_INSN_CACHE_MISS_PENALTY
    M.getOrInsertGlobal(VT_CURR_BBL_INSN_CACHE_MISS_PENALTY, Type::getInt64Ty(Ctx));
    gVar = M.getNamedGlobal(VT_CURR_BBL_INSN_CACHE_MISS_PENALTY);
    gVar->setLinkage(GlobalValue::ExternalLinkage);
    gVar->setAlignment(8);
    #endif

    #ifndef DISABLE_LOOKAHEAD
    // VT_CURR_BBID_VAR
    M.getOrInsertGlobal(VT_CURR_BBID_VAR, Type::getInt64Ty(Ctx));
    gVar = M.getNamedGlobal(VT_CURR_BBID_VAR);
    gVar->setLinkage(GlobalValue::ExternalLinkage);
    gVar->setAlignment(8);

    // VT_LOOP_ID_VAR
    M.getOrInsertGlobal(VT_CURR_LOOP_ID_VAR, Type::getInt64Ty(Ctx));
    gVar = M.getNamedGlobal(VT_CURR_LOOP_ID_VAR);
    gVar->setLinkage(GlobalValue::ExternalLinkage);
    gVar->setAlignment(8);
    #endif

    #ifdef COLLECT_STATS
    // VT_TOTAL_EXEC_BINARY_INSTRUCTIONS
    M.getOrInsertGlobal(VT_TOTAL_EXEC_BINARY_INSTRUCTIONS, Type::getInt64Ty(Ctx));
    gVar = M.getNamedGlobal(VT_TOTAL_EXEC_BINARY_INSTRUCTIONS);
    gVar->setLinkage(GlobalValue::ExternalLinkage);
    gVar->setAlignment(8);

    // VT_TOTAL_EXEC_VTL_INSTRUCTIONS
    M.getOrInsertGlobal(VT_TOTAL_EXEC_VTL_INSTRUCTIONS, Type::getInt64Ty(Ctx));
    gVar = M.getNamedGlobal(VT_TOTAL_EXEC_VTL_INSTRUCTIONS);
    gVar->setLinkage(GlobalValue::ExternalLinkage);
    gVar->setAlignment(8);
    #endif

    // VT_CALLBACK_FUNC
    auto Type = FunctionType::get(Type::getVoidTy(Ctx), false);
    Function::Create(Type, GlobalValue::ExternalLinkage, VT_CALLBACK_FUNC, &M);  

    if (ContainsMain) {
            //outs() << "Contains Main: Creating __VT_Stub function body" << "\n";
            createStubFunction(M);
    } else {

            Function::Create(FunctionType::get(Type::getVoidTy(Ctx), false),
                GlobalValue::ExternalLinkage, VT_STUB_FUNC, &M); 
    }
     
}

#ifndef DISABLE_LOOKAHEAD
void VirtualTimeManager::__acquireFlock(
    std::string lockFilePath, std::string clangInitParamsPath) {

    if (IgnoreVtCache)
        return;
    outs() << "INFO: Acquiring CLANG Lock ... \n";
    lockFd = open(lockFilePath.c_str(), O_RDWR | O_CREAT, 0666);
    int rc = 0;
    do {
         rc = flock(lockFd, LOCK_EX | LOCK_NB);
         if (rc != 0)
            usleep(10000);
    } while (rc != 0);
    outs() << "INFO: Acquired CLANG Lock ... \n";

    // create file if it does not exist !
    std::fstream fs;
      fs.open(clangInitParamsPath, std::ios::out | std::ios::app);
      fs.close();
}

void VirtualTimeManager::__releaseFlock() {
    if (IgnoreVtCache)
        return;
    flock(lockFd, LOCK_UN);
    close(lockFd);
    outs() << "INFO: Released CLANG Lock ... \n";
}
#endif

bool VirtualTimeManager::doInitialization(Module& M) {

    if (DisableVtInsertion)
        return false;
    #ifdef DISABLE_VTL
	return false;
    #endif

    std::string ttnProjectTimingModel = "EMPIRICAL";
    int ttnProjectRobSize = 1024;
    int ttnProjectNumDispatchUnits = 8;

    #ifndef DISABLE_LOOKAHEAD
    
    const char *homedir;
    if ((homedir = getenv("HOME")) == NULL) {
        homedir = getpwuid(getuid())->pw_dir;
    }
    std::string home(homedir);
    std::string ttnProjectsDBPath = home + "/.ttn/projects.db";
    

    if (access( ttnProjectsDBPath.c_str(), F_OK ) != -1) {
        auto Text = llvm::MemoryBuffer::getFileAsStream(
            ttnProjectsDBPath);
            
        StringRef Content = Text ? Text->get()->getBuffer() : "";
        outs() << "Parsing initial parameters ...\n";
        if (!Content.empty()) {
            auto E = llvm::json::parse(Content);
            if (E) {
                llvm::json::Object * O = E->getAsObject()->getObject(
                    TTN_PROJECTS_ACTIVE_KEY);
                projectArchName = O->getString(
                    TTN_PROJECTS_PROJECT_ARCH_NAME_KEY).getValue();
		ttnProjectTimingModel = O->getString(
                    TTN_PROJECTS_PROJECT_TIMING_MODEL_KEY).getValue();
		ttnProjectRobSize = O->getInteger(
                    TTN_PROJECTS_PROJECT_ROB_SIZE_KEY).getValue();
		ttnProjectNumDispatchUnits = O->getInteger(
                    TTN_PROJECTS_PROJECT_DISPATCH_UNITS_KEY).getValue();
                projectArchTimingsPath = O->getString(
                    TTN_PROJECTS_PROJECT_ARCH_TIMINGS_PATH_KEY).getValue();
                clangLockFilePath = O->getString(
                    TTN_PROJECTS_PROJECT_SRC_DIR_KEY).getValue();
                clangLockFilePath +=  "/.ttn/clang_lock";
                clangInitParamsPath = O->getString(
                    TTN_PROJECTS_PROJECT_SRC_DIR_KEY).getValue();
                clangInitParamsPath += "/.ttn/clang_init_params.json";

            } else {
                outs() << "WARN: Parse ERROR: Ignoring ttnProjectsDB ... \n";
            }
        }
    }

    __acquireFlock(clangLockFilePath, clangInitParamsPath);

    int64_t lastUsedBBL = 0;
    int64_t lastUsedLoop = 0;

    if (!IgnoreVtCache) {
        auto Text = llvm::MemoryBuffer::getFileAsStream(
            clangInitParamsPath);
            
        StringRef Content = Text ? Text->get()->getBuffer() : "";
        outs() << "Parsing initial parameters ...\n";
        if (!Content.empty()) {
            auto E = llvm::json::parse(Content);
            if (E) {
                llvm::json::Object * O = E->getAsObject();
                
                auto lastUsedBBLCounter = O->getInteger("BBL_Counter");
                auto lastUsedLoopCounter = O->getInteger("Loop_Counter");
                if (lastUsedBBLCounter.hasValue()) {
                    lastUsedBBL = *(lastUsedBBLCounter.getPointer());
                }
                if (lastUsedLoopCounter.hasValue()) {
                    lastUsedLoop = *(lastUsedLoopCounter.getPointer());
                }
            } else {
                outs() << "WARN: Parse ERROR: Ignoring initial parameters ... \n";
            }
        }
    }
    globalBBCounter = (long )lastUsedBBL;
    globalLoopCounter = (long )lastUsedLoop;
    outs() << "Clang File Lock Path: " << clangLockFilePath << "\n";
    outs() << "Clang Init Params Path: " << clangInitParamsPath << "\n";
    #else
    const char *homedir;
    if ((homedir = getenv("HOME")) == NULL) {
        homedir = getpwuid(getuid())->pw_dir;
    }
    std::string home(homedir);
    std::string ttnProjectsDBPath = home + "/.ttn/projects.db";
    if (access( ttnProjectsDBPath.c_str(), F_OK ) != -1) {
        auto Text = llvm::MemoryBuffer::getFileAsStream(
            ttnProjectsDBPath);
            
        StringRef Content = Text ? Text->get()->getBuffer() : "";
        outs() << "Parsing initial parameters ...\n";
        if (!Content.empty()) {
            auto E = llvm::json::parse(Content);
            if (E) {
                llvm::json::Object * O = E->getAsObject()->getObject(
                    TTN_PROJECTS_ACTIVE_KEY);
                projectArchName = O->getString(
                    TTN_PROJECTS_PROJECT_ARCH_NAME_KEY).getValue();
                projectArchTimingsPath = O->getString(
                    TTN_PROJECTS_PROJECT_ARCH_TIMINGS_PATH_KEY).getValue();
		ttnProjectTimingModel = O->getString(
                    TTN_PROJECTS_PROJECT_TIMING_MODEL_KEY).getValue();
		ttnProjectRobSize = O->getInteger(
                    TTN_PROJECTS_PROJECT_ROB_SIZE_KEY).getValue();
		ttnProjectNumDispatchUnits = O->getInteger(
                    TTN_PROJECTS_PROJECT_DISPATCH_UNITS_KEY).getValue();
            } else {
                outs() << "WARN: Parse ERROR: Ignoring ttnProjectsDB ... \n";
            }
        }
    }
    #endif

    outs() << "Project Arch Name: " << projectArchName << "\n";
    outs() << "Project Arch Timings Path: " << projectArchTimingsPath << "\n";
    outs() << "Using Processor timing model: " << ttnProjectTimingModel << "\n";
    outs() << "Modelled Processor Rob Size: " << ttnProjectRobSize << "\n";
    outs() << "Modelled Processor Dispatch units: " << ttnProjectNumDispatchUnits << "\n";

    outs() << "Initializing target machine timing information ... " << "\n";
    targetMachineSpecificInfo = new TargetMachineSpecificInfo(
        projectArchName, projectArchTimingsPath, ttnProjectTimingModel,
	ttnProjectRobSize, ttnProjectNumDispatchUnits);
    outs() << "INFO: Inserting Virtual time specific code ...\n";
    return false;
}

bool VirtualTimeManager::doFinalization(Module& M) {

    if (DisableVtInsertion)
        return false;

    #ifdef DISABLE_VTL
	return false;
    #endif

    #ifndef DISABLE_LOOKAHEAD
    __releaseFlock();

    // writing json output
    std::error_code EC;
    llvm::json::Object clangParamsObj;

    outs() << "Updating clang init params ..." << "\n";
    clangParamsObj.insert(
    llvm::json::Object::KV{"BBL_Counter",
        llvm::json::Value(globalBBCounter)});
    clangParamsObj.insert(
    llvm::json::Object::KV{"Loop_Counter",
        llvm::json::Value(globalLoopCounter)});

    llvm::raw_fd_ostream osClangParams(clangInitParamsPath, EC,
        llvm::sys::fs::OF_Text);
    if (EC) {
        llvm::errs() << "WARN: Could not update clang init params file: " <<
        clangInitParamsPath << " " << EC.message() << '\n';
       } else {
        osClangParams << llvm::formatv("{0:2}\n",
            json::Value(std::move(clangParamsObj)));
        clangParamsObj.clear();
    }

    std::string OutputFile = M.getSourceFileName() + ".artifacts.json";
    llvm::raw_fd_ostream OS(OutputFile, EC, llvm::sys::fs::OF_Text);
    if (EC) {
        llvm::errs() << "WARN: Could not create artifacts file: " << OutputFile 
        << " " << EC.message() << '\n';
        if (targetMachineSpecificInfo)
            delete targetMachineSpecificInfo;
        return false;
       }
    outs() << "INFO: Writing artifacts to:  " << OutputFile << "\n";
       llvm::json::Object Sarif = perModuleObj;
    OS << llvm::formatv("{0:2}\n", json::Value(std::move(Sarif)));
    perModuleObj.clear();	
    #endif

    if (targetMachineSpecificInfo)
        delete targetMachineSpecificInfo;

    return false;
}


void VirtualTimeManager::__insertVtStubFn(MachineFunction &MF) {

    const llvm::TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();
    llvm::Module &M = const_cast<Module &>(*MMI->getModule());

    MachineBasicBlock *origMBB = &MF.front();
    origMBB->clear();
    while (MF.size() > 1)
        MF.erase(std::next(MF.begin()));

    
    MachineBasicBlock* InitMBB = MF.CreateMachineBasicBlock();
    MachineBasicBlock* FunctionCallMBB = MF.CreateMachineBasicBlock();
    InitMBB->addSuccessor(FunctionCallMBB);
    InitMBB->addSuccessor(origMBB);		
    FunctionCallMBB->addSuccessor(origMBB);

    MF.push_front(FunctionCallMBB);
    MF.push_front(InitMBB);
    
    // %r11 contains the current virtual time burst length. If less than or
    // equal to zero it implies, the current execution burst has been
    // completed.
    // test %r11, %r11
    llvm::BuildMI(*InitMBB, InitMBB->end(), DebugLoc(),
        TII.get(X86::TEST64rr)).addReg(X86::R11).addReg(X86::R11);

    // jns origMBB - jmp to exit if r11 > 0
    llvm::BuildMI(*InitMBB, InitMBB->end(), DebugLoc(),
        TII.get(X86::JCC_1)).addMBB(origMBB).addImm(9);
    

    // FunctionCallMBB: (add push pop of other registers here)
    

    // push %rax
    llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(),
        TII.get(X86::PUSH64r)).addReg(X86::RAX);

    // push %rcx
    llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(),
        TII.get(X86::PUSH64r)).addReg(X86::RCX);

    // push %rdx
    llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(),
        TII.get(X86::PUSH64r)).addReg(X86::RDX);

    // push %rsi
    llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(),
        TII.get(X86::PUSH64r)).addReg(X86::RSI);

    // push %rdi
    llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(),
        TII.get(X86::PUSH64r)).addReg(X86::RDI);

    // push %r8
    llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(),
        TII.get(X86::PUSH64r)).addReg(X86::R8);

    // push %r9
    llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(),
        TII.get(X86::PUSH64r)).addReg(X86::R9);

    /* These are already potentially saved if needed in the basic block from which the stub is called. So no need to save them again. */
    /*
    // push %r10
    llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(),
        TII.get(X86::PUSH64r)).addReg(X86::R10);

    // push %r11
    llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(),
        TII.get(X86::PUSH64r)).addReg(X86::R11); */

    /* Callee saved registers. No Need to save them again. They will be automatically saved inside vtCallbackFn if needed. */
    /*
    // push %rbp - callee saved
    llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(),
        TII.get(X86::PUSH64r)).addReg(X86::RBP);

    // push %rbx - callee saved
    llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(),
        TII.get(X86::PUSH64r)).addReg(X86::RBX);

    // push %r12
    llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(),
        TII.get(X86::PUSH64r)).addReg(X86::R12);

    // push %r13
    llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(),
        TII.get(X86::PUSH64r)).addReg(X86::R13);

    // push %r14
    llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(),
        TII.get(X86::PUSH64r)).addReg(X86::R14);

    // push %r15
    llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(),
        TII.get(X86::PUSH64r)).addReg(X86::R15); */

    // callq VT_CALLBACK_FUNC
    llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(),
        TII.get(X86::CALL64pcrel32)).addGlobalAddress(
            M.getNamedValue(VT_CALLBACK_FUNC));

    /* Callee saved registers. No need to restore them */
    /*
    // pop %r15
    llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(),
        TII.get(X86::POP64r)).addReg(X86::R15);

    // pop %r14
    llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(),
        TII.get(X86::POP64r)).addReg(X86::R14);

    // pop %r13
    llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(),
        TII.get(X86::POP64r)).addReg(X86::R13);

    // pop %r12
    llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(),
        TII.get(X86::POP64r)).addReg(X86::R12);

    // pop %rbx - callee saved
    llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(),
        TII.get(X86::POP64r)).addReg(X86::RBX);

    // pop %rbp - callee saved
    llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(),
        TII.get(X86::POP64r)).addReg(X86::RBP); */
    
    /* These are already potentially saved if needed in the basic block from which the stub is called. So no need to restore them again. */
    // pop %r11
    /* llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(),
        TII.get(X86::POP64r)).addReg(X86::R11);

    // pop %r10
    llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(),
        TII.get(X86::POP64r)).addReg(X86::R10); */

    // pop %r9
    llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(),
        TII.get(X86::POP64r)).addReg(X86::R9);

    // pop %r8
    llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(),
        TII.get(X86::POP64r)).addReg(X86::R8);

    // pop %rdi
    llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(),
        TII.get(X86::POP64r)).addReg(X86::RDI);

    // pop %rsi
    llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(),
        TII.get(X86::POP64r)).addReg(X86::RSI);

    // pop %rdx
    llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(),
        TII.get(X86::POP64r)).addReg(X86::RDX);

    // pop %rcx
    llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(),
        TII.get(X86::POP64r)).addReg(X86::RCX);

    // pop %rax
    llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(),
        TII.get(X86::POP64r)).addReg(X86::RAX);


    llvm::BuildMI(*origMBB, origMBB->end(), DebugLoc(), TII.get(X86::RETQ));
}

#ifndef DISABLE_LOOKAHEAD
void VirtualTimeManager::__insertVtlLogic(MachineFunction &MF,
    MachineBasicBlock* origMBB, bool force, long blockNumber, long LoopID) {
#else
void VirtualTimeManager::__insertVtlLogic(MachineFunction &MF,
    MachineBasicBlock* origMBB, bool force) {
#endif

    const llvm::TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();
    //const llvm::TargetRegisterInfo &TRI = TRI = *MF.getSubtarget().getRegisterInfo();
    llvm::Module &M = const_cast<Module &>(*MMI->getModule());
    auto ret = targetMachineSpecificInfo->GetMBBCompletionCycles(origMBB, TII);
    long bblCyclesConsumed = (long)ret.first;
    long vtlLength = 0;

    #ifdef COLLECT_STATS
    
    long instrCount = (long)ret.second;
    #endif
    
    bool isR10Alive = origMBB->isLiveIn(X86::R10) | origMBB->isLiveIn(X86::R10D) | origMBB->isLiveIn(X86::R10W) | origMBB->isLiveIn(X86::R10B);

    // Note. It appears to be crucial to only use R10 and R11 here and not other registers. The other registers e.g RBX, RCX may be carrying function
    // arguments upon entry and liveness analysis does not track it. Further other registers such as R12 - R15, may need to be preserved across the function.
    // So we need to do more analysis to determine if any of these other registers can be used instead of R10. If you use any other register, you must push and
    // and pop that in every basic block and ignore its liveness.

    /*bool isR13Alive = origMBB->isLiveIn(X86::R13) | origMBB->isLiveIn(X86::R13D) | origMBB->isLiveIn(X86::R13W) | origMBB->isLiveIn(X86::R13B);
    bool isR14Alive = origMBB->isLiveIn(X86::R14) | origMBB->isLiveIn(X86::R14D) | origMBB->isLiveIn(X86::R14W) | origMBB->isLiveIn(X86::R14B);
    bool isR15Alive = origMBB->isLiveIn(X86::R15) | origMBB->isLiveIn(X86::R15D) | origMBB->isLiveIn(X86::R15W) | origMBB->isLiveIn(X86::R15B);*/
    bool isR11Alive = origMBB->isLiveIn(X86::R11) | origMBB->isLiveIn(X86::R11D) | origMBB->isLiveIn(X86::R11W) | origMBB->isLiveIn(X86::R11B);

    MCPhysReg secondRegister = X86::R10;
    bool isSecondRegisterLive = true;
    bool isFlagsAlive = origMBB->isLiveIn(X86::EFLAGS);
      

    if (origMBB->empty() || !origMBB->isLegalToHoistInto() || !origMBB->getBasicBlock())
	return;

    #ifndef DISABLE_LOOKAHEAD
    bool skipAddingLoopID = LoopID > 0 ? false: true;
    long currBBID = blockNumber;
    #endif

    if (!isR10Alive) {
        secondRegister = X86::R10;
        isSecondRegisterLive = false;
    } /*else if (!isR13Alive) {
	secondRegister = X86::R13;
	isSecondRegisterLive = false;
    } else if (!isR14Alive) {
	secondRegister = X86::R14;
	isSecondRegisterLive = false;
    } else if (!isR15Alive) {
	secondRegister = X86::R15;
	isSecondRegisterLive = false;
    }*/
    
    
    // origMBB:
    // restore eflags register
    // popf
    if (isFlagsAlive) {
        llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(), TII.get(X86::POPF64));
        vtlLength += 2;
    }

    // pop %r11
    if (isR11Alive || force) {
        llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(),
            TII.get(X86::POP64r)).addReg(X86::R11);
        vtlLength += 2;
    }

    // pop secondRegister
    if (isSecondRegisterLive || force) {
        llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(),
            TII.get(X86::POP64r)).addReg(secondRegister);
        vtlLength += 2;
    }
    

    #ifdef DISABLE_INSN_CACHE_SIM
    // callq VT_STUB_FUNC
    llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(),
        TII.get(X86::CALL64pcrel32)).addGlobalAddress(
            M.getNamedValue(VT_STUB_FUNC));
    vtlLength ++;
    #endif


    #ifndef DISABLE_LOOKAHEAD
    if (!skipAddingLoopID) {
        // movq LoopID (secondRegister)
        llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(),
            TII.get(X86::MOV64mi32)).addReg(secondRegister).addImm(1).addReg(0)
            .addImm(0).addReg(0).addImm(LoopID);

        // movq VT_CURR_LOOP_ID_VAR@GOTPCREL(%rip) secondRegister
        llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(),
            TII.get(X86::MOV64rm))
                .addReg(secondRegister).addReg(X86::RIP).addImm(1).addReg(0)
                .addGlobalAddress(
                    M.getNamedValue(VT_CURR_LOOP_ID_VAR), 0,
                        MO_GOTPCREL).addReg(0);
        vtlLength += 2;
    }
    

    // movq BasicBlockID (secondRegister)
    llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(),
        TII.get(X86::MOV64mi32)).addReg(secondRegister).addImm(1).addReg(0)
        .addImm(0).addReg(0).addImm(currBBID);

    // movq VT_CURR_BBID_VAR@GOTPCREL(%rip) secondRegister
    llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(), TII.get(X86::MOV64rm))
            .addReg(secondRegister).addReg(X86::RIP).addImm(1).addReg(0)
            .addGlobalAddress(
                M.getNamedValue(VT_CURR_BBID_VAR), 0, MO_GOTPCREL).addReg(0);
    vtlLength += 2;
    #endif

    #ifndef DISABLE_INSN_CACHE_SIM
    // movq bblCyclesConsumed (secondRegister)
    llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(),
        TII.get(X86::MOV64mi32)).addReg(secondRegister).addImm(1).addReg(0)
            .addImm(0).addReg(0).addImm(
                (long)ret.second);

    // movq VT_CURR_BBL_INSN_CACHE_MISS_PENALTY@GOTPCREL(%rip) secondRegister
    llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(), TII.get(X86::MOV64rm))
        .addReg(secondRegister).addReg(X86::RIP).addImm(1).addReg(0)
        .addGlobalAddress(
            M.getNamedValue(VT_CURR_BBL_INSN_CACHE_MISS_PENALTY), 0, MO_GOTPCREL).addReg(0);
    
    vtlLength += 2;
    #endif

    // movq %r11 %(secondRegister)
    llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(), TII.get(X86::MOV64mr))
            .addReg(secondRegister).addImm(1).addReg(0).addImm(0).addReg(0)
            .addReg(X86::R11);

    // subq	bblCyclesConsumed, %r11
    llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(),
        TII.get(X86::SUB64ri8)).addDef(X86::R11).addReg(X86::R11)
            .addImm(bblCyclesConsumed);


    // movq (secondRegister) %r11
    llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(),
        TII.get(X86::MOV64rm)).addDef(X86::R11).addReg(secondRegister)
            .addImm(1).addReg(0).addImm(0).addReg(0);

    // movq VT_CYCLES_COUNTER_VAR@GOTPCREL(%rip) secondRegister
    llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(),
        TII.get(X86::MOV64rm)).addReg(secondRegister).addReg(X86::RIP).addImm(1)
            .addReg(0).addGlobalAddress(
                M.getNamedValue(VT_CYCLES_COUNTER_VAR), 0, MO_GOTPCREL).addReg(0);

    vtlLength += 4;


    #ifdef COLLECT_STATS
    // movq %r11 %(secondRegister)
    llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(), TII.get(X86::MOV64mr))
            .addReg(secondRegister).addImm(1).addReg(0).addImm(0).addReg(0)
            .addReg(X86::R11);

    // addq	instrCount, %r11
    llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(),
        TII.get(X86::ADD64ri8)).addDef(X86::R11).addReg(X86::R11)
            .addImm(instrCount);


    // movq (secondRegister) %r11
    llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(),
        TII.get(X86::MOV64rm)).addDef(X86::R11).addReg(secondRegister)
            .addImm(1).addReg(0).addImm(0).addReg(0);

    // movq VT_TOTAL_EXEC_BINARY_INSTRUCTIONS@GOTPCREL(%rip) secondRegister
    llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(),
        TII.get(X86::MOV64rm)).addReg(secondRegister).addReg(X86::RIP).addImm(1)
            .addReg(0).addGlobalAddress(
                M.getNamedValue(VT_TOTAL_EXEC_BINARY_INSTRUCTIONS), 0, MO_GOTPCREL).addReg(0);


    // movq %r11 %(secondRegister)
    llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(), TII.get(X86::MOV64mr))
            .addReg(secondRegister).addImm(1).addReg(0).addImm(0).addReg(0)
            .addReg(X86::R11);

    // addq  vtlLength, %r11
    llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(),
        TII.get(X86::ADD64ri8)).addDef(X86::R11).addReg(X86::R11)
            .addImm(vtlLength);


    // movq (secondRegister) %r11
    llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(),
        TII.get(X86::MOV64rm)).addDef(X86::R11).addReg(secondRegister)
            .addImm(1).addReg(0).addImm(0).addReg(0);

    // movq VT_TOTAL_EXEC_VTL_INSTRUCTIONS@GOTPCREL(%rip) secondRegister
    llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(),
        TII.get(X86::MOV64rm)).addReg(secondRegister).addReg(X86::RIP).addImm(1)
            .addReg(0).addGlobalAddress(
                M.getNamedValue(VT_TOTAL_EXEC_VTL_INSTRUCTIONS), 0, MO_GOTPCREL).addReg(0);
    #endif

    // push secondRegister
    if (isSecondRegisterLive || force)
    llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(),
        TII.get(X86::PUSH64r)).addReg(secondRegister);

    
    // push %r11
    if (isR11Alive || force)
    llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(),
        TII.get(X86::PUSH64r)).addReg(X86::R11);

    // save eflags register
    // pushf
    if (isFlagsAlive) {
	    MachineInstr *Push = BuildMI(*origMBB, origMBB->begin(), DebugLoc(),
		TII.get(X86::PUSHF64));
	    Push->getOperand(2).setIsUndef();
    }

}



bool VirtualTimeManager::runOnMachineFunction(MachineFunction &MF) {

    
    //outs() << "##############   Function Name: " 
    // << MF.getName() << "################\n";
    MMI = &MF.getMMI();
    llvm::Module &M = const_cast<Module &>(*MMI->getModule());

    bool ret = false;
    int count = 0;
    bool ContainsMain = (M.getFunction(MAIN_FUNC) != nullptr);
    

    if (DisableVtInsertion) {
        return false;
    }

    #ifdef DISABLE_VTL
	return false;
    #endif


    #ifndef DISABLE_LOOKAHEAD
    if (MF.getName().equals(VT_CALLBACK_FUNC) ||
        MF.getName().equals(VT_LOOP_LOOKAHEAD_FUNC))
        return false;
    #else
    if (MF.getName().equals(VT_CALLBACK_FUNC))
        return false;
    #endif

    if (!MF.getName().equals(VT_STUB_FUNC) && !CreatedExternDefinitions) {
        createExternDefinitions(M, ContainsMain);
        CreatedExternDefinitions = true;
        ret = true;
    }

    if (!MF.getName().equals(VT_STUB_FUNC)) {

        #ifndef DISABLE_LOOKAHEAD
        // Already created stub function. Do other inserts here and call StubFunction
        // Get MachineLoopInfo or compute it on the fly if it's unavailable
        MLI = getAnalysisIfAvailable<MachineLoopInfo>();
        if (!MLI) {
            // Get MachineDominatorTree or compute it on the fly if it's unavailable
            MDT = getAnalysisIfAvailable<MachineDominatorTree>();
            if (!MDT) {
                OwnedMDT = std::unique_ptr<MachineDominatorTree>(
                    new MachineDominatorTree());
                OwnedMDT->getBase().recalculate(MF);
                MDT = OwnedMDT.get();
            }
            OwnedMLI = std::unique_ptr<MachineLoopInfo>(new MachineLoopInfo());
            OwnedMLI->getBase().analyze(MDT->getBase());
            MLI = OwnedMLI.get();
        }
        if (!MLI)
            outs() << "WARN: Machine Loop info analysis not available ...\n";

        //if (MLI)
        //	MLI->runOnMachineFunction(MF);
        
        MachineFunctionCFGHolder mCFGHolder(&MF, globalBBCounter,
            targetMachineSpecificInfo, globalLoopCounter);
        mCFGHolder.runOnThisMachineFunction(MLI);
        globalBBCounter = mCFGHolder.getFinalGlobalBBLCounter();
        globalLoopCounter = mCFGHolder.getFinalGlobalLoopCounter();
        for (auto &MBB : MF) {
	        count ++;
            MachineBasicBlock* origMBB = &MBB;
            __insertVtlLogic(MF, origMBB, count == 1,
                mCFGHolder.getGlobalMBBNumber(origMBB),
                mCFGHolder.getAssociatedLoopNumber(origMBB));		
        }

        perModuleObj.insert(
            llvm::json::Object::KV{
                MF.getName(),
                llvm::json::Value(std::move(mCFGHolder.getComposedMFJsonObj()))
            });	
        #else
        for (auto &MBB : MF) {
            MachineBasicBlock* origMBB = &MBB;
            count ++;
            __insertVtlLogic(MF, origMBB, count==1);		
        }
        #endif
        ret = true;	 

    } else if  (MF.getName().equals(VT_STUB_FUNC) and ContainsMain) {
        // Inside stub function. Add comparison instructions. 
        __insertVtStubFn(MF);
        ret = true;
    }

    return ret;
}

