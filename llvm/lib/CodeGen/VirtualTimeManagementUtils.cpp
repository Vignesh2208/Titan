#include "VirtualTimeManagementUtils.h"
#include "llvm/CodeGen/TargetInstrInfo.h"

using namespace llvm;


void MachineInsPrefixTree::addMachineInsn(std::string insnName,
	float avgCpuCycles) {
	struct MachineInsPrefixTreeNode * curr_node = root.get();
	std::unique_ptr<struct MachineInsPrefixTreeNode> tmp;
	for (unsigned i=0; i< insnName.length(); ++i) {
    	auto got = curr_node->children.find(insnName.at(i));
		if (got == curr_node->children.end()) {


			tmp = std::unique_ptr<struct MachineInsPrefixTreeNode>(
				new struct MachineInsPrefixTreeNode);
			tmp->component = insnName.at(i);
			tmp->avgCpuCycles = -1;

			if (i == insnName.length() - 1)
				tmp->avgCpuCycles = avgCpuCycles;
			curr_node->children.insert(std::make_pair(tmp->component,
				std::move(tmp)));
			curr_node = curr_node->children.find(insnName.at(i))->second.get();
		} else {
			curr_node = 
					(struct MachineInsPrefixTreeNode *)got->second.get();
			if ((i == insnName.length() - 1))
				curr_node->avgCpuCycles = avgCpuCycles;
		}
	}
}
float MachineInsPrefixTree::getAvgCpuCycles(std::string insnName) {
	struct MachineInsPrefixTreeNode * curr_node = root.get();
	for (unsigned i=0; i< insnName.length(); ++i) {
    	auto got = curr_node->children.find(insnName.at(i));
		if (got == curr_node->children.end())
			return curr_node->avgCpuCycles; // longest-prefix match
		curr_node = 
			(struct MachineInsPrefixTreeNode *)got->second.get();
		if ((i == insnName.length() - 1))
			return curr_node->avgCpuCycles;
	}
	return -1;
}

void MachineSpecificConfig::Initialize() {
	if (MachineArchName == DEFAULT_PROJECT_ARCH_NAME)
		return;

	int count = 0;
	if (access( MachineTimingInfoFile.c_str(), F_OK ) != -1) {
		auto Text = llvm::MemoryBuffer::getFileAsStream(
			MachineTimingInfoFile);
			
		StringRef Content = Text ? Text->get()->getBuffer() : "";
		outs() << "Parsing machine architecture timing information ...\n";
		if (!Content.empty()) {
			auto E = llvm::json::parse(Content);
			if (E) {
				llvm::json::Object * insnNames = E->getAsObject()->getObject(
					"insn_name");
				llvm::json::Object * latencyCycles = E->getAsObject()->getObject(
					"avg_latency");
				for (auto it = insnNames->begin(); it != insnNames->end(); it++){
					std::string key = it->getFirst().str();

					if (it->getSecond().getAsString().hasValue()) {
						std::string insnName = 
							it->getSecond().getAsString().getValue();
						float avgCpuCycles = -1;
						if (latencyCycles->getNumber(key).hasValue())
							avgCpuCycles = latencyCycles->getNumber(key).getValue();
						if (!count && avgCpuCycles > 0) {
							outs() << "Instruction name: " << insnName
								<< "avgCpuCycles: " << avgCpuCycles << "\n";
						}

						machineInsPrefixTree->addMachineInsn(insnName,
							avgCpuCycles);
						count ++;	
					}				
				}

			} else {
				outs() << "WARN: Parse ERROR: Ignoring Machine arch timings ... \n";
			}
		}
	}

}

float MachineSpecificConfig::GetInsnCompletionCycles(std::string insnName) {

	if (MachineArchName == DEFAULT_PROJECT_ARCH_NAME)
		return 1.0;
	float avgCpuCycles = machineInsPrefixTree->getAvgCpuCycles(insnName);
	if (avgCpuCycles < 0)
		return 1.0;

	outs () << "Requested insnName: " << insnName << " avg cpu cycles: "
			<< avgCpuCycles << "\n";
	return avgCpuCycles;
}

  
std::pair<unsigned long, unsigned long> TargetMachineSpecificInfo::GetMBBCompletionCycles(
	MachineBasicBlock * mbb, const llvm::TargetInstrInfo &TII) {
	if (!mbb)
		return std::make_pair(0, 0);
	float mbbCompletionCycles = 0;
	unsigned long InstrCount = 0;
	bool skip = false;
	for (MachineBasicBlock::instr_iterator Iter = mbb->instr_begin(),
		E = mbb->instr_end(); Iter != E; Iter++) {
		skip = false;
		MachineInstr &MI = *Iter;
		     
			
		if (MI.isCall()) {
			std::string fnName;
			for (unsigned OpIdx = 0; OpIdx  != MI.getNumOperands(); ++OpIdx) {
				const MachineOperand &MO = MI.getOperand(OpIdx);
				if (!MO.isGlobal()) continue;
				const Function * F = dyn_cast<Function>(MO.getGlobal());
				if (!F) continue;

				if (F->hasName()) {
					fnName = F->getName();
				} else {
					fnName = "__unknown__";
				}
			}

			#ifndef DISABLE_INSN_CACHE_SIM
			if (fnName == INS_CACHE_CALLBACK_FN)
				skip = true;
			#endif

			#ifndef DISABLE_DATA_CACHE_SIM
			if ((fnName == DATA_READ_CACHE_CALLBACK_FN ||
				fnName == DATA_WRITE_CACHE_CALLBACK_FN))
				skip = true;
			#endif
			
			#ifndef DISABLE_LOOKAHEAD
			if (fnName == VT_LOOP_LOOKAHEAD_FUNC)
				skip = true;
			#endif
		}

		if (!MI.isCFIInstruction() && !skip) {
			mbbCompletionCycles += machineConfig->GetInsnCompletionCycles(
				TII.getName(MI.getOpcode())); 
			InstrCount ++;
		}
	}

	// since we are dealing with integers, we cap it to atleast 1.
	// otherwise there might be a case where a basic block is looped infinitely
	// with zero time elapse.
	// TODO: fix this so that burst completion cycles are tracked as floats
	// instead of long
	if (mbbCompletionCycles < 1)
		mbbCompletionCycles = 1;
	return std::make_pair((long)mbbCompletionCycles, InstrCount);
}


#ifndef DISABLE_LOOKAHEAD

MachineLoop * MachineLoopHolder::getMachineLoopContainingPreheader(
            MachineLoop * Parent, MachineBasicBlock * preHeader) {
	const std::vector<MachineLoop *> *Vec = &Parent->getSubLoops();

	MachineLoop * ret = nullptr;
	if (Parent->getLoopPreheader() == preHeader) {
			return Parent;
	}
	if ((*Vec).empty()) {
		return nullptr;
	}

	for(auto iter = (*Vec).begin(); iter != (*Vec).end(); iter++) {
        MachineLoop * currLoop = (*iter);
		if (!currLoop)
			continue;
		ret = getMachineLoopContainingPreheader(currLoop, preHeader);
		if (ret)
			return ret;
    }

	return ret;
}
void MachineLoopHolder::setLoopPreheader(long loopNumber,
	MachineBasicBlock * mbb) {
	long preHeaderBBLNumber = mCFGHolder->getGlobalMBBNumber(mbb);
	assert(!loopPreheaders.count(preHeaderBBLNumber));
	loopPreheaders.insert(std::make_pair(preHeaderBBLNumber, loopNumber));

}

long MachineLoopHolder::getAssociatedLoopNumber(long preHeaderBBLNumber) {
	if (loopPreheaders.find(preHeaderBBLNumber) == loopPreheaders.end())
		return -1;
	return loopPreheaders.find(preHeaderBBLNumber)->second;
}

void MachineLoopHolder::addLoopHeader(
	long loopNumber, MachineBasicBlock * header) {
	long headerBBLNumber = mCFGHolder->getGlobalMBBNumber(header);
	if (!loopHeaders.count(loopNumber)) {
		// key does not exist
		loopHeaders.insert(std::make_pair(
			loopNumber, std::set<long>()));
	}
	loopHeaders.at(loopNumber).insert(headerBBLNumber);
}

void MachineLoopHolder::addLoopLatch(
	long loopNumber, MachineBasicBlock * latch) {
	long latchBBLNumber = mCFGHolder->getGlobalMBBNumber(latch);
	if (!loopLatches.count(loopNumber)) {
		// key does not exist
		loopLatches.insert(std::make_pair(
			loopNumber, std::set<long>()));
	}
	loopLatches.at(loopNumber).insert(latchBBLNumber);

}
void MachineLoopHolder::updateStructuredLoopCalledFunctions(
	long loopNumber, MachineBasicBlock * bblInLoop) {
	long bblNumber = mCFGHolder->getGlobalMBBNumber(bblInLoop);

	if (mCFGHolder->calledFunctions.count(bblNumber)) {
		if (!loopCalledFunctions.count(loopNumber)) {
			// key does not exist
			loopCalledFunctions.insert(std::make_pair(
				loopNumber, std::set<std::string>()));
		}
		for (auto it = mCFGHolder->calledFunctions.at(bblNumber).begin();
			it != mCFGHolder->calledFunctions.at(bblNumber).end(); it++) {
			if (*it != VT_STUB_FUNC && *it != VT_LOOP_LOOKAHEAD_FUNC)
				loopCalledFunctions.at(loopNumber).insert(*it);
		}
	}
}

void MachineLoopHolder::updateUnstructuredLoopCalledFunctions(long loopNumber) {
	if (!loopHeaders.count(loopNumber))
		return;
	for (auto it = loopHeaders.at(loopNumber).begin();
		it != loopHeaders.at(loopNumber).end(); it++) {
		long loopHeaderMBBNumber = *it;
		if (!bblToSCC.count(loopHeaderMBBNumber))
			continue;
		long sccID = bblToSCC.find(loopHeaderMBBNumber)->second;
		if (!sccCalledFunctions.count(sccID))
			continue;
		if (!loopCalledFunctions.count(loopNumber)) {
			// key does not exist
			loopCalledFunctions.insert(std::make_pair(
				loopNumber, std::set<std::string>()));
		}
		for (auto setIt = sccCalledFunctions.at(sccID).begin();
			setIt != sccCalledFunctions.at(sccID).end(); setIt++) {
			if (*setIt != VT_STUB_FUNC && *setIt != VT_LOOP_LOOKAHEAD_FUNC)
				loopCalledFunctions.at(loopNumber).insert(*setIt);
		}
	}
}

void MachineLoopHolder::processAllLoops() {
	int foundLoop = 0, foundStructuredLoop = 0, numSuccessors = 0;
	int sccID = 0;
	if (associatedMF->getName().equals(VT_STUB_FUNC)) 
		return;
	for (scc_iterator<MachineFunction *> I = scc_begin(associatedMF),
		IE = scc_end(associatedMF); I != IE; ++I) {
		// Obtain the vector of BBs in this SCC and print it out.
		const std::vector<MachineBasicBlock *> &SCCMBBs = *I;
		sccCalledFunctions.insert(std::make_pair(sccID,
			std::set<std::string>()));
		for (std::vector<MachineBasicBlock *>::const_iterator BBI = SCCMBBs.begin(),
			BBIE = SCCMBBs.end(); BBI != BBIE; ++BBI) {
			long bblNumber = mCFGHolder->getGlobalMBBNumber((*BBI));
			bblToSCC.insert(std::make_pair(bblNumber, sccID));

			if (mCFGHolder->calledFunctions.count(bblNumber)) {
				for (auto it = mCFGHolder->calledFunctions.at(bblNumber).begin();
					it != mCFGHolder->calledFunctions.at(bblNumber).end(); it++) {
					sccCalledFunctions.at(sccID).insert(*it);
				}
			}
		}
		sccID ++;
	}
	// Already created stub function. Do other inserts here and
	// call StubFunction
	for (auto &MBB : *associatedMF) {
		MachineBasicBlock* origMBB = &MBB;
		foundLoop = 0, foundStructuredLoop = 0, numSuccessors = 0;
		numSuccessors = origMBB->succ_size();
		for (MachineBasicBlock::instr_iterator Iter = MBB.instr_begin(),
			E = MBB.instr_end(); Iter != E; Iter++) {
			
			MachineInstr &Inst = *Iter;
			
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

				if (fnName == VT_LOOP_LOOKAHEAD_FUNC) {
					// This basic block contains loop lookahead function.
					foundLoop = 1;
					globalLoopCounter ++;
					setLoopPreheader(globalLoopCounter, origMBB);
				}
			}
		}

		if (!foundLoop) {
			continue;
		}	


		for (MachineBasicBlock *succ : origMBB->successors()) {
			
			MachineLoop *Loop = 
				mCFGHolder->mLoopInfo ? mCFGHolder->mLoopInfo->getLoopFor(succ): nullptr;
							
			if (Loop &&
				getMachineLoopContainingPreheader(Loop, origMBB) == Loop &&
				numSuccessors == 1) {
				outs() << "INFO: Loop-Preheader sanity check passed ...\n";

				llvm::SmallVector<MachineBasicBlock *, 4> latches;
				Loop->getLoopLatches(latches);
				
				if (latches.empty() || !Loop->getHeader())
					outs() << "WARN: Loop has no latch or header blocks ! Skipping ...\n";
				else {
					for (auto it = latches.begin(); it != latches.end(); it++) {
						addLoopLatch(globalLoopCounter, *it);
					}
					addLoopHeader(globalLoopCounter, Loop->getHeader());
					for (MachineBasicBlock* blk: Loop->blocks())
						updateStructuredLoopCalledFunctions(globalLoopCounter, blk);
				}
				
				foundStructuredLoop = 1;
				break;
			} else {
				addLoopHeader(globalLoopCounter, succ);
				for (MachineBasicBlock *pred : succ->predecessors()) {
					if (pred != origMBB)
						addLoopLatch(globalLoopCounter, pred);
				}
				foundStructuredLoop = 0;
			}
		}

		if (!foundStructuredLoop)
			updateUnstructuredLoopCalledFunctions(globalLoopCounter);
	}
	
}

llvm::json::Object MachineLoopHolder::getLoopJsonObj(long loopNumber) {
	llvm::json::Array loop_headers;
	llvm::json::Array loop_latches;
	llvm::json::Array loop_called_functions;
	llvm::json::Object loopObj;

	assert(loopHeaders.find(loopNumber) != loopHeaders.end());
	assert(loopLatches.find(loopNumber) != loopLatches.end());
	

	for (auto it  = loopHeaders.find(loopNumber)->second.begin();
		it != loopHeaders.find(loopNumber)->second.end(); it++) {
		loop_headers.push_back(llvm::json::Value(*it));
	}
	for (auto it  = loopLatches.find(loopNumber)->second.begin();
		it != loopLatches.find(loopNumber)->second.end(); it++) {
		loop_latches.push_back(llvm::json::Value(*it));
	}

	if (loopCalledFunctions.find(loopNumber) != loopCalledFunctions.end()) {
		for (auto it  = loopCalledFunctions.find(loopNumber)->second.begin();
			it != loopCalledFunctions.find(loopNumber)->second.end(); it++) {
			loop_called_functions.push_back(*it);
		}
	}
	loopObj.insert(llvm::json::Object::KV{"loop_headers",
		llvm::json::Value(std::move(loop_headers))});
	loopObj.insert(llvm::json::Object::KV{"loop_latches",
		llvm::json::Value(std::move(loop_latches))});
	loopObj.insert(llvm::json::Object::KV{"loop_called_fns",
		llvm::json::Value(std::move(loop_called_functions))});
	return loopObj;
}

llvm::json::Object MachineLoopHolder::getComposedLoopJsonObj() {
	llvm::json::Object loopComposedObj;
	for (auto it = loopHeaders.begin(); it != loopHeaders.end(); it ++) {
		long loopNumber = it->first;
		loopComposedObj.insert(
			llvm::json::Object::KV{std::to_string(loopNumber),
								   getLoopJsonObj(loopNumber)});
	}
	return loopComposedObj;
}

llvm::json::Array MachineFunctionCFGHolder::getAllEdgesFromBBL(long bblNumber) {
    llvm::json::Array bblEdges;

	if (outEdges.find(bblNumber) == outEdges.end())
		return bblEdges;

	for (auto successor : outEdges.find(bblNumber)->second) {
		bblEdges.push_back(llvm::json::Value(successor));	
	}
	return bblEdges;
} 

llvm::json::Array MachineFunctionCFGHolder::getAllEdgesToBBL(long bblNumber) {
    llvm::json::Array bblEdges;
    if (inEdges.find(bblNumber) == inEdges.end())
		return bblEdges;

	for (auto predecessor : inEdges.find(bblNumber)->second) {
		bblEdges.push_back(llvm::json::Value(predecessor));	
	}
	return bblEdges;
}

llvm::json::Array MachineFunctionCFGHolder::getAllCalledFnsFromBBL(
    long bblNumber) {
    llvm::json::Array calledFuncs;

	if (calledFunctions.find(bblNumber) == calledFunctions.end())
		return calledFuncs;

	for(auto calledFn: calledFunctions.find(bblNumber)->second) {
		calledFuncs.push_back(calledFn);
	}
	return calledFuncs;
}

llvm::json::Object MachineFunctionCFGHolder::composeBBLObject(long bblNumber) {
    long bblWeight;
	llvm::json::Object bblObj;

    bblWeight = getBBLWeight(bblNumber);
	bblObj.insert(llvm::json::Object::KV{"out_edges",
        getAllEdgesFromBBL(bblNumber)});
	bblObj.insert(llvm::json::Object::KV{"in_edges",
        getAllEdgesToBBL(bblNumber)});
	bblObj.insert(llvm::json::Object::KV{"called_fns",
        getAllCalledFnsFromBBL(bblNumber)});
	bblObj.insert(llvm::json::Object::KV{"weight",
        llvm::json::Value(bblWeight)});

	return bblObj;
}


void MachineFunctionCFGHolder::runOnThisMachineFunction(MachineLoopInfo * MLI) {
	
    if (!associatedMF || globalBBLCounter< 0)
        return;
    int blockNumber = 0;
    int InstrCount;
    if (associatedMF->getName().equals(VT_STUB_FUNC) ||
		associatedMF->getName().equals(VT_LOOP_LOOKAHEAD_FUNC))
		return;
	mLoopInfo = MLI;
	const llvm::TargetInstrInfo &TII = *(associatedMF->getSubtarget().getInstrInfo());
	// Already created stub function. Do other inserts here and
	// call StubFunction
	for (auto &MBB : *associatedMF) {
		globalBBLCounter ++;
		blockNumber ++;
		InstrCount = 0;
		MachineBasicBlock* origMBB = &MBB;

		if (blockNumber == 1)
			entryBBLNumber = globalBBLCounter;
		localToGlobalBBL.insert(
			std::make_pair(origMBB->getNumber(), globalBBLCounter));

		if (origMBB->isReturnBlock()) {
			// block ends in a return instruction
			addReturningBlock(globalBBLCounter);
		}
		for (MachineBasicBlock::instr_iterator Iter = MBB.instr_begin(),
			E = MBB.instr_end(); Iter != E; Iter++) {
			
			MachineInstr &Inst = *Iter;
			if (!Inst.isCFIInstruction())
				InstrCount ++;	     
			
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

				addCalledFunction(globalBBLCounter, fnName);
			}
		}

		setBBLWeight(globalBBLCounter,
			targetMachineSpecificInfo->GetMBBCompletionCycles(origMBB, TII).first);					
	}


	// add bbl edges ...
	for (auto &MBB : *associatedMF) {
		MachineBasicBlock* currMBB = &MBB;
		long currMBBGlobalNumber = getGlobalMBBNumber(currMBB);
		if (currMBBGlobalNumber < 0)
			continue;

		for (MachineBasicBlock * successorBlock : MBB.successors()) {
			addOutEdge(currMBB, successorBlock);
			
		}
		for (MachineBasicBlock * predecessorBlock : MBB.predecessors()) {
			addInEdge(currMBB, predecessorBlock);
		}
	}

	// process all loops
	if (mLoopInfo)
		mloopHolder->processAllLoops();
}

long MachineFunctionCFGHolder::getGlobalMBBNumber(MachineBasicBlock * mbb) {
	if (!mbb)
		return -1;
	auto ret = localToGlobalBBL.find(mbb->getNumber());
	assert(ret != localToGlobalBBL.end());
	return ret->second;
}

long MachineFunctionCFGHolder::getAssociatedLoopNumber(MachineBasicBlock * mbb) {
	if (!mloopHolder || !mbb)
		return -1;
	return mloopHolder->getAssociatedLoopNumber(getGlobalMBBNumber(mbb));
}
void MachineFunctionCFGHolder::addCalledFunction(
    long bblNumber, std::string fnName) {
    
	if (fnName == VT_STUB_FUNC || fnName == VT_LOOP_LOOKAHEAD_FUNC)
		return;

    if (calledFunctions.count(bblNumber)) {
        // key already exists
        calledFunctions.at(bblNumber).push_back(fnName);
    } else {
        calledFunctions.insert(std::make_pair(bblNumber,
            std::vector<std::string>()));
        calledFunctions.at(bblNumber).push_back(fnName);
    }
}

void MachineFunctionCFGHolder::addOutEdge(
    MachineBasicBlock * from, MachineBasicBlock * to) {
	long fromGlobalBBLNumber = getGlobalMBBNumber(from);
	long toGlobalBBLNumber = getGlobalMBBNumber(to);

	if (!outEdges.count(fromGlobalBBLNumber)) {
		// key does not exist
		outEdges.insert(std::make_pair(
			fromGlobalBBLNumber, std::vector<long>()));
	}
	outEdges.at(fromGlobalBBLNumber).push_back(toGlobalBBLNumber);
}

void MachineFunctionCFGHolder::addInEdge(
    MachineBasicBlock *to, MachineBasicBlock * from) {
	long fromGlobalBBLNumber = getGlobalMBBNumber(from);
	long toGlobalBBLNumber = getGlobalMBBNumber(to);

	if (!inEdges.count(toGlobalBBLNumber)) {
		// key does not exist
		inEdges.insert(std::make_pair(
			toGlobalBBLNumber, std::vector<long>()));
	}
	inEdges.at(toGlobalBBLNumber).push_back(fromGlobalBBLNumber);
}

void MachineFunctionCFGHolder::setBBLWeight(long bblNumber, long weight) {
	bblWeights.insert(std::make_pair(bblNumber, weight));
}

long MachineFunctionCFGHolder::getBBLWeight(MachineBasicBlock * mbb) {
	return getBBLWeight(this->getGlobalMBBNumber(mbb));
}

long MachineFunctionCFGHolder::getBBLWeight(long bblNumber) {
	auto ret = bblWeights.find(bblNumber);
	if (ret == bblWeights.end())
		return -1;
	return ret->second;
}

void MachineFunctionCFGHolder::addReturningBlock(long bblNumber) {
	returningBlocks.push_back(bblNumber);
}

llvm::json::Object MachineFunctionCFGHolder::getComposedMFJsonObj() {
	llvm::json::Array returning_blocks;
	llvm::json::Object all_bbls;
	llvm::json::Object mf_obj;

	for (auto element : bblWeights) {
		all_bbls.insert(
			llvm::json::Object::KV{
				std::to_string(element.first),
				composeBBLObject(element.first)
			});
	}
	for (unsigned int i = 0; i < returningBlocks.size(); i++) {
		returning_blocks.push_back(returningBlocks[i]);
	}

	mf_obj.insert(
		llvm::json::Object::KV{
			"bbls", llvm::json::Value(std::move(all_bbls))
		});
	mf_obj.insert(
		llvm::json::Object::KV{
			"returning_blocks", llvm::json::Value(std::move(returning_blocks))
		});
	mf_obj.insert(
		llvm::json::Object::KV{
			"entry_block", llvm::json::Value(entryBBLNumber)});
	if (mloopHolder) {
		mf_obj.insert(
			llvm::json::Object::KV{
				"loops", llvm::json::Value(std::move(
					mloopHolder->getComposedLoopJsonObj()))
			});
	}
	return mf_obj;
}

#endif
