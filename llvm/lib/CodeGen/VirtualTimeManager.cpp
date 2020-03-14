#include "VirtualTimeManager.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/Pass.h"

#include <unordered_map>




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

/*
VirtualTimeManager::VirtualTimeManager() : llvm::MachineFunctionPass(ID) {
	Printed = false;
}

bool VirtualTimeManager::runOnMachineFunction(llvm::MachineFunction &fn) {
	const llvm::TargetInstrInfo &TII = *fn.getSubtarget().getInstrInfo();
	MachineBasicBlock& bb = *fn.begin();
	
        if (DisableVtInsertion) {
		outs() << "Virtual time insertion is disabled" << "\n";
		return false;
	}

	if (!Printed) {
		outs() << "Running VirtualTimeManager Pass on Function" << fn.getName() << "\n";
		Printed = true;
	}

	llvm::BuildMI(bb, bb.begin(), llvm::DebugLoc(), TII.get(llvm::X86::NOOP));
	return true;
}*/

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
       Function::Create(Type, GlobalValue::LinkOnceODRLinkage, stubFunctionName, &M);
   F->setVisibility(GlobalValue::DefaultVisibility);
   F->setComdat(M.getOrInsertComdat(stubFunctionName));
 
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
  
   // vtInsnCounterGlobalVar
   M.getOrInsertGlobal(vtInsnCounterGlobalVar, Type::getInt64Ty(Ctx));
   auto *gVar = M.getNamedGlobal(vtInsnCounterGlobalVar);
   gVar->setLinkage(GlobalValue::ExternalLinkage);
   gVar->setAlignment(8);

   // vtCurrBBSizeVar
   M.getOrInsertGlobal(vtCurrBBSizeVar, Type::getInt64Ty(Ctx));
   gVar = M.getNamedGlobal(vtCurrBBSizeVar);
   gVar->setLinkage(GlobalValue::ExternalLinkage);
   gVar->setAlignment(8);

   // vtCurrBasicBlockIDVar
   M.getOrInsertGlobal(vtCurrBasicBlockIDVar, Type::getInt64Ty(Ctx));
   gVar = M.getNamedGlobal(vtCurrBasicBlockIDVar);
   gVar->setLinkage(GlobalValue::ExternalLinkage);
   gVar->setAlignment(8);

   // enabled
   M.getOrInsertGlobal(enabled, Type::getInt32Ty(Ctx));
   gVar = M.getNamedGlobal(enabled);
   gVar->setLinkage(GlobalValue::ExternalLinkage);
   gVar->setAlignment(4);

   // vtControlFunctionName
   auto Type = FunctionType::get(Type::getVoidTy(Ctx), false);
   Function::Create(Type, GlobalValue::ExternalLinkage, vtControlFunctionName, &M);  

   if (ContainsMain) {
        outs() << "Contains Main: Creating __VT_Stub function body" << "\n";
	createStubFunction(M);
   } else {
	// declare stub as extern
        //outs() << "Does not contain Main: Declaring __X86_Stub as extern" << "\n";
        Function::Create(FunctionType::get(Type::getVoidTy(Ctx), false), GlobalValue::ExternalLinkage, stubFunctionName, &M); 
   }
	 
}

bool VirtualTimeManager::runOnMachineFunction(MachineFunction &MF) {

    const llvm::TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();
    //outs() << "##############   Function Name: " << MF.getName() << "################\n";
    MMI = &MF.getMMI();
    llvm::Module &M = const_cast<Module &>(*MMI->getModule());

    bool ret = false;
    int blockNumber = 0;
    llvm::StringRef mainFnName = "main";
    //llvm::StringRef targetGvar = "global_var_3";
    bool ContainsMain = (M.getFunction(mainFnName) != nullptr);
    int InstrCount;
    std::string sourceFileName = M.getSourceFileName();

    std::hash<std::string> hasher;

    if (!DisableVtInsertion) {
	if (!Printed) {
		outs() << "#### Inserting Virtual time specific code #####" << "\n";
                //auto *gVar = M.getNamedGlobal(targetGvar);
		//printGlobalVar(gVar);
		Printed = true;
	}
	
    } else if (DisableVtInsertion) {
	return false;
    }


    if (vtControlFunctionName.equals(MF.getName()))
	return false;

    if (!stubFunctionName.equals(MF.getName()) && !CreatedExternDefinitions) {
	createExternDefinitions(M, ContainsMain);
	CreatedExternDefinitions = true;
        ret = true;
    }

    if (!stubFunctionName.equals(MF.getName())) {
	// Already created stub function. Do other inserts here and call StubFunction
	ret = true;

	for (auto &MBB : MF) {
		InstrCount = 0;
		for (MachineBasicBlock::instr_iterator Iter = MBB.instr_begin(), E = MBB.instr_end(); Iter != E; Iter++) {
	     	     MachineInstr &Inst = *Iter;
		     if (!Inst.isCFIInstruction()) {
		     	InstrCount ++;	    
		     } 
		}

		globalBBCounter ++;

		long currBBID = std::abs((long)hasher(sourceFileName + std::to_string(globalBBCounter)));

		MachineBasicBlock* origMBB = &MBB;
			
		// origMBB:
		// popf 
		llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(), TII.get(X86::POPF64));

		// pop %rdx
		//llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(), TII.get(X86::POP64r)).addReg(X86::RDX);

		// popf %rcx
		//llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(), TII.get(X86::POP64r)).addReg(X86::RCX);

		// addq $0x18, %rsp
		llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(), TII.get(X86::ADD64ri8)).addReg(X86::RSP).addReg(X86::RSP).addImm(0x18);


		// mov 0x10(%rsp), %rcx
		llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(), TII.get(X86::MOV64rm))
			.addReg(X86::RCX).addReg(X86::RSP).addImm(1).addReg(0).addImm(0x10).addReg(0);

		// mov 0x8(%rsp), %rdx
		llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(), TII.get(X86::MOV64rm))
			.addReg(X86::RDX).addReg(X86::RSP).addImm(1).addReg(0).addImm(0x8).addReg(0);

		// callq stubFunctionName
		llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(), TII.get(X86::CALL64pcrel32)).addGlobalAddress(M.getNamedValue(stubFunctionName));

		// movq enabled@GOTPCREL(%rip) %rcx
		llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(), TII.get(X86::MOV64rm))
			      .addReg(X86::RCX).addReg(X86::RIP).addImm(1).addReg(0).addGlobalAddress(M.getNamedValue(enabled), 0, MO_GOTPCREL).addReg(0);

		// movq BasicBlockID (%rcx)
		llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(), TII.get(X86::MOV64mi32))
				.addReg(X86::RCX).addImm(1).addReg(0).addImm(0).addReg(0).addImm(currBBID);

		// movq vtCurrBasicBlockIDVar@GOTPCREL(%rip) %rcx
		llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(), TII.get(X86::MOV64rm))
				.addReg(X86::RCX).addReg(X86::RIP).addImm(1).addReg(0).addGlobalAddress(M.getNamedValue(vtCurrBasicBlockIDVar), 0, MO_GOTPCREL).addReg(0);

		// movq InstrCount (%rcx)
		llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(), TII.get(X86::MOV64mi32))
				.addReg(X86::RCX).addImm(1).addReg(0).addImm(0).addReg(0).addImm(InstrCount);

		// movq vtCurrBBSizeVar@GOTPCREL(%rip) %rcx
		llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(), TII.get(X86::MOV64rm))
				.addReg(X86::RCX).addReg(X86::RIP).addImm(1).addReg(0).addGlobalAddress(M.getNamedValue(vtCurrBBSizeVar), 0, MO_GOTPCREL).addReg(0);

		// movq %rcx %(rdx)
		llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(), TII.get(X86::MOV64mr))
				.addReg(X86::RDX).addImm(1).addReg(0).addImm(0).addReg(0).addReg(X86::RCX);

		// subq	InstrCount, %rcx
		llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(), TII.get(X86::SUB64ri8)).addDef(X86::RCX).addReg(X86::RCX).addImm(InstrCount);


		// movq (%rdx) %rcx
		llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(), TII.get(X86::MOV64rm)).addDef(X86::RCX).addReg(X86::RDX).addImm(1).addReg(0).addImm(0).addReg(0);

		// movq vtInsnCounterGlobalVar@GOTPCREL(%rip) %rdx
		llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(), TII.get(X86::MOV64rm))
				.addReg(X86::RDX).addReg(X86::RIP).addImm(1).addReg(0).addGlobalAddress(M.getNamedValue(vtInsnCounterGlobalVar), 0, MO_GOTPCREL).addReg(0);


		// push %rcx
		//llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(), TII.get(X86::PUSH64r)).addReg(X86::RCX);


		// push %rdx
		//llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(), TII.get(X86::PUSH64r)).addReg(X86::RDX);

		// mov %rcx 0x10(%rsp)
		llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(), TII.get(X86::MOV64mr))
			.addReg(X86::RSP).addImm(1).addReg(0).addImm(0x10).addReg(0).addReg(X86::RCX);

		// mov %rdx 0x8(%rsp)
		llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(), TII.get(X86::MOV64mr))
			.addReg(X86::RSP).addImm(1).addReg(0).addImm(0x8).addReg(0).addReg(X86::RDX);
		
		// subq $0x18, %rsp
		llvm::BuildMI(*origMBB, origMBB->begin(), DebugLoc(), TII.get(X86::SUB64ri8)).addReg(X86::RSP).addReg(X86::RSP).addImm(0x18);

		// pushf
		MachineInstr *Push = BuildMI(*origMBB, origMBB->begin(), DebugLoc(), TII.get(X86::PUSHF64));
		Push->getOperand(2).setIsUndef();
	}

    } else if  (stubFunctionName.equals(MF.getName()) and ContainsMain) {
	// Inside stub function. Add comparison instructions. 
	ret = true;
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

		
	// cmpl $0 (%rcx) - for checking if enabled
	llvm::BuildMI(*InitMBB, InitMBB->end(), DebugLoc(), TII.get(X86::CMP64mi8))
			.addReg(X86::RCX).addImm(1).addReg(0).addImm(0).addReg(0).addImm(0);

	// jne FunctionCallMBB
	llvm::BuildMI(*InitMBB, InitMBB->end(), DebugLoc(), TII.get(X86::JCC_1)).addMBB(FunctionCallMBB).addImm(5);
	
	// InitFalseMBB:
	// cmpq $0 (%rdx) - for checking if insnCount finished
	llvm::BuildMI(*InitFalseMBB, InitFalseMBB->end(), DebugLoc(), TII.get(X86::CMP64mi8))
			.addReg(X86::RDX).addImm(1).addReg(0).addImm(0).addReg(0).addImm(0);

	// jg origMBB
	llvm::BuildMI(*InitFalseMBB, InitFalseMBB->end(), DebugLoc(), TII.get(X86::JCC_1)).addMBB(origMBB).addImm(15);

	// FunctionCallMBB: (add push pop of other registers here)

	/*
	// push %rbp
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::PUSH64r)).addReg(X86::RBP);

	// push %rbx
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::PUSH64r)).addReg(X86::RBX);

	// push %rax
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::PUSH64r)).addReg(X86::RAX);

	// push %rsi
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::PUSH64r)).addReg(X86::RSI);

	// push %rdi
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::PUSH64r)).addReg(X86::RDI);

	// push %r8
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::PUSH64r)).addReg(X86::R8);

	// push %r9
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::PUSH64r)).addReg(X86::R9);

	// push %r10
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::PUSH64r)).addReg(X86::R10);

	// push %r11
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::PUSH64r)).addReg(X86::R11);

	// push %r12
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::PUSH64r)).addReg(X86::R12);

	// push %r13
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::PUSH64r)).addReg(X86::R13);

	// push %r14
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::PUSH64r)).addReg(X86::R14);

	// push %r15
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::PUSH64r)).addReg(X86::R15);
	*/

	// subq $0xf8, %rsp
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::SUB64ri32)).addReg(X86::RSP).addReg(X86::RSP).addImm(0x100);

	// mov %rbp 0x8(%rsp)
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::MOV64mr))
		.addReg(X86::RSP).addImm(1).addReg(0).addImm(0x8).addReg(0).addReg(X86::RBP);

	// mov %rbx 0x10(%rsp)
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::MOV64mr))
		.addReg(X86::RSP).addImm(1).addReg(0).addImm(0x10).addReg(0).addReg(X86::RBX);

	// mov %rax 0x18(%rsp)
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::MOV64mr))
		.addReg(X86::RSP).addImm(1).addReg(0).addImm(0x18).addReg(0).addReg(X86::RAX);

	// mov %rsi 0x20(%rsp)
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::MOV64mr))
		.addReg(X86::RSP).addImm(1).addReg(0).addImm(0x20).addReg(0).addReg(X86::RSI);

	// mov %rdi 0x28(%rsp)
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::MOV64mr))
		.addReg(X86::RSP).addImm(1).addReg(0).addImm(0x28).addReg(0).addReg(X86::RDI);

	// mov %r8 0x30(%rsp)
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::MOV64mr))
		.addReg(X86::RSP).addImm(1).addReg(0).addImm(0x30).addReg(0).addReg(X86::R8);

	// mov %r9 0x38(%rsp)
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::MOV64mr))
		.addReg(X86::RSP).addImm(1).addReg(0).addImm(0x38).addReg(0).addReg(X86::R9);

	// mov %r10 0x40(%rsp)
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::MOV64mr))
		.addReg(X86::RSP).addImm(1).addReg(0).addImm(0x40).addReg(0).addReg(X86::R10);

	// mov %r11 0x48(%rsp)
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::MOV64mr))
		.addReg(X86::RSP).addImm(1).addReg(0).addImm(0x48).addReg(0).addReg(X86::R11);

	// mov %r12 0x50(%rsp)
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::MOV64mr))
		.addReg(X86::RSP).addImm(1).addReg(0).addImm(0x50).addReg(0).addReg(X86::R12);

	// mov %r13 0x58(%rsp)
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::MOV64mr))
		.addReg(X86::RSP).addImm(1).addReg(0).addImm(0x58).addReg(0).addReg(X86::R13);

	// mov %r14 0x60(%rsp)
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::MOV64mr))
		.addReg(X86::RSP).addImm(1).addReg(0).addImm(0x60).addReg(0).addReg(X86::R14);

	// mov %r15 0x68(%rsp)
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::MOV64mr))
		.addReg(X86::RSP).addImm(1).addReg(0).addImm(0x68).addReg(0).addReg(X86::R15);

	// mov %xmm0 0x80(%rsp)
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::MOVUPSmr))
		.addReg(X86::RSP).addImm(1).addReg(0).addImm(0x80).addReg(0).addReg(X86::XMM0);

	// mov %xmm1 0x90(%rsp)
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::MOVUPSmr))
		.addReg(X86::RSP).addImm(1).addReg(0).addImm(0x90).addReg(0).addReg(X86::XMM1);

	// mov %xmm2 0xa0(%rsp)
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::MOVUPSmr))
		.addReg(X86::RSP).addImm(1).addReg(0).addImm(0xa0).addReg(0).addReg(X86::XMM2);

	// mov %xmm3 0xb0(%rsp)
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::MOVUPSmr))
		.addReg(X86::RSP).addImm(1).addReg(0).addImm(0xb0).addReg(0).addReg(X86::XMM3);

	// mov %xmm4 0xc0(%rsp)
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::MOVUPSmr))
		.addReg(X86::RSP).addImm(1).addReg(0).addImm(0xc0).addReg(0).addReg(X86::XMM4);

	// mov %xmm5 0xd0(%rsp)
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::MOVUPSmr))
		.addReg(X86::RSP).addImm(1).addReg(0).addImm(0xd0).addReg(0).addReg(X86::XMM5);

	// mov %xmm6 0xe0(%rsp)
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::MOVUPSmr))
		.addReg(X86::RSP).addImm(1).addReg(0).addImm(0xe0).addReg(0).addReg(X86::XMM6);

	// mov %xmm7 0xf0(%rsp)
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::MOVUPSmr))
		.addReg(X86::RSP).addImm(1).addReg(0).addImm(0xf0).addReg(0).addReg(X86::XMM7);

	// callq vtControlFunctionName
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::CALL64pcrel32)).addGlobalAddress(M.getNamedValue(vtControlFunctionName));

	// movq 0x8(%rsp), %rbp
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::MOV64rm))
		.addReg(X86::RBP).addReg(X86::RSP).addImm(1).addReg(0).addImm(0x8).addReg(0);

	// movq 0x10(%rsp), %rbx
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::MOV64rm))
		.addReg(X86::RBX).addReg(X86::RSP).addImm(1).addReg(0).addImm(0x10).addReg(0);

	// movq 0x18(%rsp), %rax
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::MOV64rm))
		.addReg(X86::RAX).addReg(X86::RSP).addImm(1).addReg(0).addImm(0x18).addReg(0);

	// movq 0x20(%rsp), %rsi
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::MOV64rm))
		.addReg(X86::RSI).addReg(X86::RSP).addImm(1).addReg(0).addImm(0x20).addReg(0);

	// movq 0x28(%rsp), %rdi
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::MOV64rm))
		.addReg(X86::RDI).addReg(X86::RSP).addImm(1).addReg(0).addImm(0x28).addReg(0);

	// movq 0x30(%rsp), %r8
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::MOV64rm))
		.addReg(X86::R8).addReg(X86::RSP).addImm(1).addReg(0).addImm(0x30).addReg(0);

	// movq 0x38(%rsp), %r9
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::MOV64rm))
		.addReg(X86::R9).addReg(X86::RSP).addImm(1).addReg(0).addImm(0x38).addReg(0);

	// movq 0x40(%rsp), %r10
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::MOV64rm))
		.addReg(X86::R10).addReg(X86::RSP).addImm(1).addReg(0).addImm(0x40).addReg(0);

	// movq 0x48(%rsp), %r11
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::MOV64rm))
		.addReg(X86::R11).addReg(X86::RSP).addImm(1).addReg(0).addImm(0x48).addReg(0);

	// movq 0x50(%rsp), %r12
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::MOV64rm))
		.addReg(X86::R12).addReg(X86::RSP).addImm(1).addReg(0).addImm(0x50).addReg(0);

	// movq 0x58(%rsp), %r13
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::MOV64rm))
		.addReg(X86::R13).addReg(X86::RSP).addImm(1).addReg(0).addImm(0x58).addReg(0);

	// movq 0x60(%rsp), %r14
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::MOV64rm))
		.addReg(X86::R14).addReg(X86::RSP).addImm(1).addReg(0).addImm(0x60).addReg(0);

	// movq 0x68(%rsp), %r15
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::MOV64rm))
		.addReg(X86::R15).addReg(X86::RSP).addImm(1).addReg(0).addImm(0x68).addReg(0);

	// mov 0x80(%rsp), %xmm0
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::MOVUPSrm))
		.addReg(X86::XMM0).addReg(X86::RSP).addImm(1).addReg(0).addImm(0x80).addReg(0);

	// mov 0x90(%rsp), %xmm1
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::MOVUPSrm))
		.addReg(X86::XMM1).addReg(X86::RSP).addImm(1).addReg(0).addImm(0x90).addReg(0);

	// mov 0xa0(%rsp), %xmm2
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::MOVUPSrm))
		.addReg(X86::XMM2).addReg(X86::RSP).addImm(1).addReg(0).addImm(0xa0).addReg(0);

	// mov 0xb0(%rsp), %xmm3
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::MOVUPSrm))
		.addReg(X86::XMM3).addReg(X86::RSP).addImm(1).addReg(0).addImm(0xb0).addReg(0);

	// mov 0xc0(%rsp), %xmm4
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::MOVUPSrm))
		.addReg(X86::XMM4).addReg(X86::RSP).addImm(1).addReg(0).addImm(0xc0).addReg(0);

	// mov 0xd0(%rsp), %xmm5
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::MOVUPSrm))
		.addReg(X86::XMM5).addReg(X86::RSP).addImm(1).addReg(0).addImm(0xd0).addReg(0);

	// mov 0xe0(%rsp), %xmm6
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::MOVUPSrm))
		.addReg(X86::XMM6).addReg(X86::RSP).addImm(1).addReg(0).addImm(0xe0).addReg(0);

	// mov 0xf0(%rsp), %xmm7
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::MOVUPSrm))
		.addReg(X86::XMM7).addReg(X86::RSP).addImm(1).addReg(0).addImm(0xf0).addReg(0);



	// addq $0xf8, %rsp
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::ADD64ri32)).addReg(X86::RSP).addReg(X86::RSP).addImm(0x100);

	/*
	// pop %r15
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::POP64r)).addReg(X86::R15);

	// pop %r14
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::POP64r)).addReg(X86::R14);

	// pop %r13
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::POP64r)).addReg(X86::R13);

	// pop %r12
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::POP64r)).addReg(X86::R12);

	// pop %r11
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::POP64r)).addReg(X86::R11);

	// pop %r10
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::POP64r)).addReg(X86::R10);

	// pop %r9
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::POP64r)).addReg(X86::R9);

	// pop %r8
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::POP64r)).addReg(X86::R8);

	// pop %rdi
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::POP64r)).addReg(X86::RDI);

	// pop %rsi
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::POP64r)).addReg(X86::RSI);

	// pop %rax
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::POP64r)).addReg(X86::RAX);

	// pop %rbx
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::POP64r)).addReg(X86::RBX);

	// pop %rbp
	llvm::BuildMI(*FunctionCallMBB, FunctionCallMBB->end(), DebugLoc(), TII.get(X86::POP64r)).addReg(X86::RBP);
	*/


	// origMBB:
	// retq
	llvm::BuildMI(*origMBB, origMBB->end(), DebugLoc(), TII.get(X86::RETQ));
    }

    return ret;
}


/*
INITIALIZE_PASS(X86MachineInstrPrinter, "x86-machineinstr-printer",
    X86_MACHINEINSTR_PRINTER_PASS_NAME,
    false, // is CFG only?
    false  // is analysis?
)

namespace llvm {

FunctionPass *createX86MachineInstrPrinterPass() { return new X86MachineInstrPrinter(); }

}*/
