
#include <vector>
#include <memory>
#include <unordered_map>
#include <set>
#include "llvm/Support/JSON.h"
#include "llvm/ADT/SCCIterator.h"
#include "VirtualTimeManagementIncludes.h"



namespace llvm {

  class MachineBasicBlock;
  class TargetInstrInfo;
  
  struct MachineInsPrefixTreeNode {
      char component;    
      float avgCpuCycles;
      std::unordered_map<char,
        std::unique_ptr<struct MachineInsPrefixTreeNode>> children;
  };

  class MachineInsPrefixTree {
    private:
      std::unique_ptr<struct MachineInsPrefixTreeNode> root;
    public:
      MachineInsPrefixTree() {
        root = std::unique_ptr<struct MachineInsPrefixTreeNode>(
          new struct MachineInsPrefixTreeNode);
        root->component = '\0';
        root->avgCpuCycles = -1;
      };

      void addMachineInsn(std::string insnName, float avgCpuCycles);
      float getAvgCpuCycles(std::string insnName);
  };
  
  class MachineSpecificConfig {
    private:
      std::string MachineArchName;
      std::string MachineTimingInfoFile;
      std::unique_ptr<MachineInsPrefixTree> machineInsPrefixTree;
      
    public:
      MachineSpecificConfig(
        std::string MachineArchName, std::string MachineTimingInfoFile)
      : MachineTimingInfoFile(MachineTimingInfoFile) {
          machineInsPrefixTree = std::unique_ptr<MachineInsPrefixTree>(
            new MachineInsPrefixTree());
        };

      void Initialize();
      float GetInsnCompletionCycles(std::string insnName);
  };

  class TargetMachineSpecificInfo {
    private:
      MachineSpecificConfig * machineConfig;

    public:
      TargetMachineSpecificInfo(
        std::string MachineArchName="",
        std::string MachineTimingInfoFile="") {
          machineConfig = new MachineSpecificConfig(
            MachineArchName, MachineTimingInfoFile);
          machineConfig->Initialize();
      };

      std::pair<unsigned long, unsigned long> GetMBBCompletionCycles(
        MachineBasicBlock * mbb, const llvm::TargetInstrInfo &TII);
      ~TargetMachineSpecificInfo() { delete machineConfig; };
  };

  #ifndef DISABLE_LOOKAHEAD
  class MachineDominatorTree;  
  class MachineLoopInfo;
  class MachineLoop;
  class MachineFunction;
  class MachineFunctionCFGHolder;

  class MachineLoopHolder {
    private:
        std::unordered_map<long, std::set<std::string>> sccCalledFunctions;
        std::unordered_map<long, long> bblToSCC;
        std::unordered_map<long, long> loopPreheaders;
        std::unordered_map<long, std::set<long>> loopHeaders;
        std::unordered_map<long, std::set<long>> loopLatches;
        std::unordered_map<long, std::set<std::string>> loopCalledFunctions;
        MachineFunction * associatedMF;
        MachineFunctionCFGHolder * mCFGHolder;
        long globalLoopCounter;

        MachineLoop * getMachineLoopContainingPreheader(
            MachineLoop * Parent, MachineBasicBlock * preHeader);

        void setLoopPreheader(long loopNumber, MachineBasicBlock * mbb);
        void addLoopHeader(long loopNumber, MachineBasicBlock * header);
        void addLoopLatch(long loopNumber, MachineBasicBlock * latch);
        void updateStructuredLoopCalledFunctions(long loopNumber,
            MachineBasicBlock * bblInLoop);
        void updateUnstructuredLoopCalledFunctions(long loopNumber);
        llvm::json::Object getLoopJsonObj(long loopNumber);
    public:
        MachineLoopHolder(
            MachineFunction * associatedMF,
            MachineFunctionCFGHolder * mCFGHolder,
            long globalLoopCounter)
                : associatedMF(associatedMF), mCFGHolder(mCFGHolder),
                globalLoopCounter(globalLoopCounter) {};

        void processAllLoops();
        long getAssociatedLoopNumber(long preHeaderBBLNumber);
        long getFinalGlobalLoopCounter() { return globalLoopCounter; };
        llvm::json::Object getComposedLoopJsonObj();
      
  };



  class MachineFunctionCFGHolder {
    private:
      
      std::unordered_map<long, std::vector<long>> outEdges;
      std::unordered_map<long, std::vector<long>> inEdges;
      std::vector<long> returningBlocks;
      MachineLoopHolder * mloopHolder = nullptr;
      llvm::json::Array getAllEdgesFromBBL(long bblNumber); 
      llvm::json::Array getAllEdgesToBBL(long bblNumber);
      llvm::json::Array getAllCalledFnsFromBBL(long bblNumber);
      llvm::json::Object composeBBLObject(long bblNumber);
      std::unordered_map<long, long> bblWeights;
      std::unordered_map<long, long> localToGlobalBBL;
      MachineFunction * associatedMF = nullptr;
      long globalBBLCounter;
      TargetMachineSpecificInfo * targetMachineSpecificInfo = nullptr;
      long entryBBLNumber;
    public:
      std::unordered_map<long, std::vector<std::string>> calledFunctions;
      MachineLoopInfo * mLoopInfo = nullptr;

      MachineFunctionCFGHolder(
        MachineFunction * associatedMF, long globalBBLCounter,
        TargetMachineSpecificInfo * targetMachineSpecificInfo,
        long globalLoopCounter)
        : associatedMF(associatedMF), globalBBLCounter(globalBBLCounter),
          targetMachineSpecificInfo(targetMachineSpecificInfo) {
              mloopHolder = new MachineLoopHolder(associatedMF, this,
                globalLoopCounter);
                entryBBLNumber = 0;
          };
      void runOnThisMachineFunction(MachineLoopInfo * MLI);
      long getFinalGlobalBBLCounter() { return globalBBLCounter; }
      long getFinalGlobalLoopCounter() {
          return mloopHolder->getFinalGlobalLoopCounter(); }
      long getGlobalMBBNumber(MachineBasicBlock * mbb);
      long getAssociatedLoopNumber(MachineBasicBlock * mbb);
      void addOutEdge(MachineBasicBlock * from, MachineBasicBlock * to);
      void addInEdge(MachineBasicBlock *to, MachineBasicBlock * from);
      void addCalledFunction(long bblNumber, std::string fnName);
      void setBBLWeight(long bblNumber, long weight);
      long getBBLWeight(long bblNumber);
      void addReturningBlock(long bblNumber);
      long getBBLWeight(MachineBasicBlock * mbb);
      llvm::json::Object getComposedMFJsonObj();
      ~MachineFunctionCFGHolder() { delete mloopHolder; };
  };

  #endif
}