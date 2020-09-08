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

#undef DISABLE_LOOKAHEAD
#undef DISABLE_INSN_CACHE_SIM
#undef DISABLE_DATA_CACHE_SIM

#define MAIN_FUNC "main"
#define VT_STUB_FUNC "__Vt_Stub"
#define VT_CALLBACK_FUNC "vtCallbackFn"
#define VT_CYCLES_COUNTER_VAR "currBurstCyclesLeft"
#define VT_CURR_BBID_VAR "currBBID"

#ifndef DISABLE_LOOKAHEAD
#define VT_CURR_LOOP_ID_VAR "currLoopID"
#define VT_LOOP_LOOKAHEAD_FUNC "SetLoopLookahead"
#endif

#ifndef DISABLE_INSN_CACHE_SIM
#define VT_CURR_BBL_INSN_CACHE_MISS_PENALTY  "currBBInsCacheMissPenalty"
#define INS_CACHE_CALLBACK_FN "insCacheCallback"
#endif

#ifndef DISABLE_DATA_CACHE_SIM
#define DATA_READ_CACHE_CALLBACK_FN "dataReadCacheCallback"
#define DATA_WRITE_CACHE_CALLBACK_FN "dataWriteCacheCallback"
#endif

#define CLANG_FILE_LOCK  "/tmp/clang_lock"
#define CLANG_INIT_PARAMS "/tmp/clang_init_params.json"

#define TTN_PROJECTS_ACTIVE_KEY "active"
#define TTN_PROJECTS_PROJECT_SRC_DIR_KEY "PROJECT_SRC_DIR"
#define TTN_PROJECTS_PROJECT_ARCH_NAME_KEY "PROJECT_ARCH_NAME"
#define TTN_PROJECTS_PROJECT_ARCH_TIMINGS_PATH_KEY "PROJECT_ARCH_TIMINGS_PATH"

#define DEFAULT_PROJECT_ARCH_NAME "NONE"


#endif