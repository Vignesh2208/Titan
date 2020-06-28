#include "VirtualTimeManager.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/Pass.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"






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

  
   // VT_INSN_COUNTER_VAR
   M.getOrInsertGlobal(VT_INSN_COUNTER_VAR, Type::getInt64Ty(Ctx));
   auto *gVar = M.getNamedGlobal(VT_INSN_COUNTER_VAR);
   gVar->setLinkage(GlobalValue::ExternalLinkage);
   gVar->setAlignment(8);

   // VT_CURR_BBL_SIZE_VAR
   M.getOrInsertGlobal(VT_CURR_BBL_SIZE_VAR, Type::getInt64Ty(Ctx));
   gVar = M.getNamedGlobal(VT_CURR_BBL_SIZE_VAR);
   gVar->setLinkage(GlobalValue::ExternalLinkage);
   gVar->setAlignment(8);

   // VT_CURR_BBID_VAR
   M.getOrInsertGlobal(VT_CURR_BBID_VAR, Type::getInt64Ty(Ctx));
   gVar = M.getNamedGlobal(VT_CURR_BBID_VAR);
   gVar->setLinkage(GlobalValue::ExternalLinkage);
   gVar->setAlignment(8);

   // VT_FORCE_INVOKE_CALLBACK_VAR
   M.getOrInsertGlobal(VT_FORCE_INVOKE_CALLBACK_VAR, Type::getInt32Ty(Ctx));
   gVar = M.getNamedGlobal(VT_FORCE_INVOKE_CALLBACK_VAR);
   gVar->setLinkage(GlobalValue::ExternalLinkage);
   gVar->setAlignment(4);

   // VT_CALLBACK_FUNC
   auto Type = FunctionType::get(Type::getVoidTy(Ctx), false);
   Function::Create(Type, GlobalValue::ExternalLinkage, VT_CALLBACK_FUNC, &M);  

   if (ContainsMain) {
        outs() << "Contains Main: Creating __VT_Stub function body" << "\n";
		createStubFunction(M);
   } else {

		Function::Create(FunctionType::get(Type::getVoidTy(Ctx), false),
			GlobalValue::ExternalLinkage, VT_STUB_FUNC, &M); 
   }
	 
}

void VirtualTimeManager::__acquireFlock() {
	lockFd = open("/tmp/llvm.lock", O_RDWR | O_CREAT, 0666);
	int rc = 0;
	do {
		 rc = flock(lockFd, LOCK_EX | LOCK_NB);
		 if (rc != 0)
			usleep(100000);
	} while (rc != 0);
	outs() << "Acquired File Lock \n";

	// create file if it does not exist !
	std::fstream fs;
  	fs.open("/tmp/llvm_init_params.json", std::ios::out | std::ios::app);
  	fs.close();
}

void VirtualTimeManager::__releaseFlock() {
	flock(lockFd, LOCK_UN);
	close(lockFd);
	outs() << "Released File Lock \n";
}

bool VirtualTimeManager::doInitialization(Module& M) {

	__acquireFlock();
	auto Text = llvm::MemoryBuffer::getFileAsStream(
		"/tmp/llvm_init_params.json");
		
	StringRef Content = Text ? Text->get()->getBuffer() : "";

	int64_t lastUsedBBL = 0;
	int64_t lastUsedLoop = 0;

	outs() << "Read Json Content: " << Content << "\n";
	if (!Content.empty()) {
		auto E = llvm::json::parse(Content);
		if (E) {
			llvm:json::Object * O = E->getAsObject();
			
			auto lastUsedBBLCounter = O->getInteger("BBL_Counter");
			auto lastUsedLoopCounter = O->getInteger("Loop_Counter");
			if (lastUsedBBLCounter.hasValue()) {
				lastUsedBBL = *(lastUsedBBLCounter.getPointer());
			}
			if (lastUsedLoopCounter.hasValue()) {
				lastUsedLoop = *(lastUsedLoopCounter.getPointer());
			}
			outs() << "Last Used BBL = " << lastUsedBBL << " Last Used Loop = " << lastUsedLoop << "\n";
		} else {
			outs() << "JSON parse error \n";
		}
	}

	globalBBCounter = (long )lastUsedBBL;

	if (!DisableVtInsertion) {
		outs() << "~~~~ Inserting Virtual time specific code ~~~~~" << "\n";
    }
	return false;
}

bool VirtualTimeManager::doFinalization(Module& M) {
	__releaseFlock();

	// writing json output
	std::error_code EC;
	std::string OutputFile = M.getSourceFileName() + ".artifacts.json";
	llvm::raw_fd_ostream OS(OutputFile, EC, llvm::sys::fs::OF_Text);
	if (EC) {
		llvm::errs() << "warning: could not create file: " << OutputFile 
		<< " " << EC.message() << '\n';
		return false;
   	}
	outs() << "Writing json output to:  " << OutputFile << "\n";
   	llvm::json::Object Sarif = perModuleObj;
	OS << llvm::formatv("{0:2}\n", json::Value(std::move(Sarif)));
	perModuleObj.clear();
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
	MachineBasicBlock* InitFalseMBB = MF.CreateMachineBasicBlock();
	InitMBB->addSuccessor(FunctionCallMBB);
	InitMBB->addSuccessor(InitFalseMBB);		
	InitFalseMBB->addSuccessor(origMBB);
	InitFalseMBB->addSuccessor(FunctionCallMBB);
	FunctionCallMBB->addSuccessor(origMBB);

	MF.push_front(FunctionCallMBB);
	MF.push_front(InitFalseMBB);
	MF.push_front(InitMBB);
	
	// test %rbx, %rbx
	llvm::BuildMI(*InitMBB, InitMBB->end(), DebugLoc(), TII.get(X86::TEST32rr))
			.addReg(X86::EBX).addReg(X86::EBX);

	// jne FunctionCallMBB - jmp to FunctionCall if ebx != 0
	llvm::BuildMI(*InitMBB, InitMBB->end(), DebugLoc(),
		TII.get(X86::JCC_1)).addMBB(FunctionCallMBB).addImm(5);

	// test %rdx, %rdx
	llvm::BuildMI(*InitFalseMBB, InitFalseMBB->end(), DebugLoc(),
		TII.get(X86::TEST64rr)).addReg(X86::RDX).addReg(X86::RDX);

	// jns origMBB - jmp to exit if rdx > 0
	llvm::BuildMI(*InitFalseMBB, InitFalseMBB->end(), DebugLoc(),
		TII.get(X86::JCC_1)).addMBB(origMBB).addImm(9);
	

	// FunctionCallMBB: (add push pop of other registers here)
	// push %rbp
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(),
		TII.get(X86::PUSH64r)).addReg(X86::RBP);

	// push %rax
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(),
		TII.get(X86::PUSH64r)).addReg(X86::RAX);

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

	// push %r10
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(),
		TII.get(X86::PUSH64r)).addReg(X86::R10);

	// push %r11
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(),
		TII.get(X86::PUSH64r)).addReg(X86::R11);


	// callq VT_CALLBACK_FUNC
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(),
		TII.get(X86::CALL64pcrel32)).addGlobalAddress(
			M.getNamedValue(VT_CALLBACK_FUNC));

	
	// pop %r11
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(),
		TII.get(X86::POP64r)).addReg(X86::R11);

	// pop %r10
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(),
		TII.get(X86::POP64r)).addReg(X86::R10);

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

	// pop %rax
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(),
		TII.get(X86::POP64r)).addReg(X86::RAX);

	// pop %rbp
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(),
		TII.get(X86::POP64r)).addReg(X86::RBP);

	llvm::BuildMI(*origMBB, origMBB->end(), DebugLoc(), TII.get(X86::RETQ));
}

void VirtualTimeManager::__insertVtlLogic(MachineFunction &MF,
	MachineBasicBlock* origMBB, long blockNumber, int bbTimeConsumed) {

	const llvm::TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();
	llvm::Module &M = const_cast<Module &>(*MMI->getModule());

	long currBBID = blockNumber;
	// origMBB:

	// pop %rax
	llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(),
		TII.get(X86::POP64r)).addReg(X86::RAX);

	// popf 
	llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(), TII.get(X86::POPF64));

	// push %rax
	llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(),
		TII.get(X86::PUSH64r)).addReg(X86::RAX);

	// pop %rax
	llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(),
		TII.get(X86::POP64r)).addReg(X86::RAX);


	// pop %rdx
	llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(),
		TII.get(X86::POP64r)).addReg(X86::RDX);

	// pop %rcx
	llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(),
		TII.get(X86::POP64r)).addReg(X86::RCX);

	// pop %rbx
	llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(),
		TII.get(X86::POP64r)).addReg(X86::RBX);

	// callq VT_STUB_FUNC
	llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(),
		TII.get(X86::CALL64pcrel32)).addGlobalAddress(
			M.getNamedValue(VT_STUB_FUNC));

			
	// movq (%rcx) %rbx
	llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(),
		TII.get(X86::MOV64rm)).addDef(X86::RBX)
			.addReg(X86::RCX).addImm(1).addReg(0).addImm(0).addReg(0);
	
	// movq VT_FORCE_INVOKE_CALLBACK_VAR@GOTPCREL(%rip) %rcx
	llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(), TII.get(X86::MOV64rm))
			.addReg(X86::RCX).addReg(X86::RIP).addImm(1).addReg(0)
			.addGlobalAddress(
				M.getNamedValue(VT_FORCE_INVOKE_CALLBACK_VAR), 0, MO_GOTPCREL)
				.addReg(0);

	// movq BasicBlockID (%rcx)
	llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(),
		TII.get(X86::MOV64mi32)).addReg(X86::RCX).addImm(1).addReg(0)
		.addImm(0).addReg(0).addImm(currBBID);

	// movq VT_CURR_BBID_VAR@GOTPCREL(%rip) %rcx
	llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(), TII.get(X86::MOV64rm))
			.addReg(X86::RCX).addReg(X86::RIP).addImm(1).addReg(0)
			.addGlobalAddress(
				M.getNamedValue(VT_CURR_BBID_VAR), 0, MO_GOTPCREL).addReg(0);

	// movq bbTimeConsumed (%rcx)
	llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(),
		TII.get(X86::MOV64mi32)).addReg(X86::RCX).addImm(1).addReg(0)
			.addImm(0).addReg(0).addImm(bbTimeConsumed);

	// movq VT_CURR_BBL_SIZE_VAR@GOTPCREL(%rip) %rcx
	llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(), TII.get(X86::MOV64rm))
		.addReg(X86::RCX).addReg(X86::RIP).addImm(1).addReg(0)
		.addGlobalAddress(
			M.getNamedValue(VT_CURR_BBL_SIZE_VAR), 0, MO_GOTPCREL).addReg(0);

	// movq %rdx %(rcx)
	llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(), TII.get(X86::MOV64mr))
			.addReg(X86::RCX).addImm(1).addReg(0).addImm(0).addReg(0)
			.addReg(X86::RDX);

	// subq	bbTimeConsumed, %rdx
	llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(),
		TII.get(X86::SUB64ri8)).addDef(X86::RDX).addReg(X86::RDX)
			.addImm(bbTimeConsumed);


	// movq (%rcx) %rdx
	llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(),
		TII.get(X86::MOV64rm)).addDef(X86::RDX).addReg(X86::RCX)
			.addImm(1).addReg(0).addImm(0).addReg(0);

	// movq VT_INSN_COUNTER_VAR@GOTPCREL(%rip) %rcx
	llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(),
		TII.get(X86::MOV64rm)).addReg(X86::RCX).addReg(X86::RIP).addImm(1)
			.addReg(0).addGlobalAddress(
				M.getNamedValue(VT_INSN_COUNTER_VAR), 0, MO_GOTPCREL).addReg(0);

		
	// push %rbx
	llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(),
		TII.get(X86::PUSH64r)).addReg(X86::RBX);

	// push %rcx
	llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(),
		TII.get(X86::PUSH64r)).addReg(X86::RCX);

	// push %rdx
	llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(),
		TII.get(X86::PUSH64r)).addReg(X86::RDX);

	// push %rax
	llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(),
		TII.get(X86::PUSH64r)).addReg(X86::RAX);

	// pop %rax
	llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(), 
		TII.get(X86::POP64r)).addReg(X86::RAX);

	// pushf
	MachineInstr *Push = BuildMI(*origMBB, origMBB->begin(), DebugLoc(),
		TII.get(X86::PUSHF64));
	Push->getOperand(2).setIsUndef();

	// push %rax
	llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(),
		TII.get(X86::PUSH64r)).addReg(X86::RAX);

}

llvm::json::Array VirtualTimeManager::getAllEdgesFromBBL(long bblNumber,
	std::unordered_map<long, std::vector<long>>& edges) {
	llvm::json::Array bblEdges;

	if (edges.find(bblNumber) == edges.end())
		return bblEdges;

	for (auto successor : edges.find(bblNumber)->second) {
		bblEdges.push_back(llvm::json::Value(successor));	
	}
	return bblEdges;
}

llvm::json::Array VirtualTimeManager::getAllCalledFnsFromBBL(long bblNumber,
	std::unordered_map<long, std::vector<std::string>>& calledFunctions) {
	llvm::json::Array calledFuncs;

	if (calledFunctions.find(bblNumber) == calledFunctions.end())
		return calledFuncs;

	for(auto calledFn: calledFunctions.find(bblNumber)->second) {
		calledFuncs.push_back(calledFn);
	}
	return calledFuncs;
}

llvm::json::Object VirtualTimeManager::composeBBLObject(long bblNumber,
	std::unordered_map<long, std::vector<std::string>>& calledFunctions,
	std::unordered_map<long, std::vector<long>>& edges, long bblWeight) {

	llvm::json::Object bblObj;

	bblObj.insert(llvm::json::Object::KV{"edges", getAllEdgesFromBBL(bblNumber, edges)});
	bblObj.insert(llvm::json::Object::KV{"called_fns", getAllCalledFnsFromBBL(bblNumber,
		calledFunctions)});
	bblObj.insert(llvm::json::Object::KV{"weight", llvm::json::Value(bblWeight)});

	return bblObj;
}

bool VirtualTimeManager::runOnMachineFunction(MachineFunction &MF) {

    
    //outs() << "##############   Function Name: " 
	// << MF.getName() << "################\n";
    MMI = &MF.getMMI();
    llvm::Module &M = const_cast<Module &>(*MMI->getModule());

    bool ret = false;
    int blockNumber = 0;
    bool ContainsMain = (M.getFunction(MAIN_FUNC) != nullptr);
    int InstrCount;
    std::string sourceFileName = M.getSourceFileName();

    std::unordered_map<long, std::vector<std::string>> calledFunctions;
	std::unordered_map<long, std::vector<long>> edges;
	std::unordered_map<long, long> bblWeights;
	std::vector<long> returningBlocks;
	std::unordered_map<int, long> localToGlobalBBL;

    if (DisableVtInsertion) {
		return false;
    }


	if (MF.getName().equals(VT_CALLBACK_FUNC))
		return false;

    if (!MF.getName().equals(VT_STUB_FUNC) && !CreatedExternDefinitions) {
		createExternDefinitions(M, ContainsMain);
		CreatedExternDefinitions = true;
        ret = true;
    }

    if (!MF.getName().equals(VT_STUB_FUNC)) {
		// Already created stub function. Do other inserts here and call StubFunction
		ret = true;

		for (auto &MBB : MF) {
			globalBBCounter ++;
			blockNumber ++;

			MachineBasicBlock* origMBB = &MBB;
			localToGlobalBBL.insert(std::make_pair(origMBB->getNumber(),
								globalBBCounter));

			if (origMBB->isReturnBlock()) {
				// block ends in a return instruction
				returningBlocks.push_back(globalBBCounter);
			}

			InstrCount = 0;
			for (MachineBasicBlock::instr_iterator Iter = MBB.instr_begin(),
				E = MBB.instr_end(); Iter != E; Iter++) {
					MachineInstr &Inst = *Iter;
				if (!Inst.isCFIInstruction()) {
					InstrCount ++;	    
				} 

				
				if (Inst.isCall()) {
					std::string fnName;
					for (unsigned OpIdx = 0; OpIdx  != Inst.getNumOperands();
						++OpIdx) {
						const MachineOperand &MO = Inst.getOperand(OpIdx);
						if (!MO.isGlobal()) continue;
						const Function * F = dyn_cast<Function>(MO.getGlobal());
						if (!F) continue;

						if (F->hasName()) {
							fnName = F->getName();
						} else {
							fnName = "__unknown__";
						}
					}

					if (calledFunctions.count(globalBBCounter)) {
						// key already exists
						calledFunctions.at(globalBBCounter).push_back(fnName);
					} else {
						calledFunctions.insert(std::make_pair(
							globalBBCounter, std::vector<std::string>()));
						calledFunctions.at(globalBBCounter).push_back(fnName);
					}
				}
			}

			bblWeights.insert(std::make_pair(globalBBCounter, InstrCount));

			// check the impact of this later ...
			//if (blockNumber <= 1)
			//	continue;

			__insertVtlLogic(MF, origMBB, globalBBCounter, InstrCount);			
		}


		// add edges ...
		for (auto &MBB : MF) {
			MachineBasicBlock* currMBB = &MBB;
			long currMBBGlobalNumber = localToGlobalBBL.find(
				currMBB->getNumber())->second;
			
			for (MachineBasicBlock * successorBlock : MBB.successors()) {
				long successorBlockGlobalNumber = localToGlobalBBL.find(
					successorBlock->getNumber())->second;

				if (edges.count(currMBBGlobalNumber)) {
					// key already exists
					edges.at(currMBBGlobalNumber).push_back(
						successorBlockGlobalNumber);
				} else {
					edges.insert(std::make_pair(
						currMBBGlobalNumber, std::vector<long>()));
					edges.at(currMBBGlobalNumber).push_back(
						successorBlockGlobalNumber);
				}
   			}
		}

		llvm::json::Array returning_blocks;
		llvm::json::Object all_bbls;
		llvm::json::Object mf_obj;

		for (auto element : bblWeights) {
			all_bbls.insert(
				llvm::json::Object::KV{
					std::to_string(element.first),
					composeBBLObject(
						element.first, calledFunctions, edges,
						element.second)
				});
		}
		for (int i = 0; i < returningBlocks.size(); i++) {
			returning_blocks.push_back(returningBlocks[i]);
		}

		mf_obj.insert(llvm::json::Object::KV{"bbls", llvm::json::Value(std::move(all_bbls))});
		mf_obj.insert(llvm::json::Object::KV{"returning_blocks", llvm::json::Value(std::move(returning_blocks))});
		// Iterate over an unordered_map using range based for loop

		perModuleObj.insert(llvm::json::Object::KV{MF.getName(), llvm::json::Value(std::move(mf_obj)) });		 

	} else if  (MF.getName().equals(VT_STUB_FUNC) and ContainsMain) {
		// Inside stub function. Add comparison instructions. 
		ret = true;
		__insertVtStubFn(MF);
    }

    return ret;
}

