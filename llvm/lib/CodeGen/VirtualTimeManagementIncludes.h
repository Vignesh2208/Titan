#ifndef __VT_MANAGEMENT_INCLUDES_H
#define __VT_MANAGEMENT_INCLUDES_H


#include <sys/file.h>
#include <unistd.h>
#include <fstream>
#include <unordered_map>

#include "llvm/ADT/SmallVector.h"

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"

#include "llvm/CodeGen/MachineJumpTableInfo.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineDominators.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"

#include "llvm/Pass.h"
#include <llvm/PassRegistry.h>

#include "llvm/Target/TargetMachine.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include "llvm/Support/JSON.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"

#define MAIN_FUNC "main"
#define VT_STUB_FUNC "__Vt_Stub"
#define VT_CALLBACK_FUNC "vtCallbackFn"
#define VT_INSN_COUNTER_VAR "currBurstLength"
#define VT_CURR_BBID_VAR "currBBID"
#define VT_CURR_BBL_SIZE_VAR  "currBBSize"
#define VT_CURR_LOOP_ID_VAR "currLoopID"
#define VT_FORCE_INVOKE_CALLBACK_VAR "alwaysOn"
#define VT_LOOP_LOOKAHEAD_FUNC "SetLoopLookahead"

#define CLANG_FILE_LOCK  "/tmp/llvm.lock"
#define CLANG_INIT_PARAMS "/tmp/llvm_init_params.json"

#define INS_CACHE_CALLBACK_FN "insCacheCallback"
#define DATA_READ_CACHE_CALLBACK_FN "dataReadCacheCallback"
#define DATA_WRITE_CACHE_CALLBACK_FN "dataWriteCacheCallback"

#endif