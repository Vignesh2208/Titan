EXTRA_CFLAGS += 
KERNEL_SRC:= /lib/modules/$(shell uname -r)/build
SUBDIR= $(shell pwd)
GCC:=gcc
RM:=rm
nCpus=$(shell nproc --all)

BUILD_CONFIG_FLAGS  = 

DISABLE_LOOKAHEAD := $(shell grep -nr "DISABLE_LOOKAHEAD" $(HOME)/Titan/CONFIG | awk -F "=" '{print $$2}' | tr -d '[ \n]')
DISABLE_INSN_CACHE_SIM := $(shell grep -nr "DISABLE_INSN_CACHE_SIM" $(HOME)/Titan/CONFIG | awk -F "=" '{print $$2}')
DISABLE_DATA_CACHE_SIM := $(shell grep -nr "DISABLE_DATA_CACHE_SIM" $(HOME)/Titan/CONFIG | awk -F "=" '{print $$2}')


$(info USED CONFIGURATION FOR COMPILATION:)
$(info DISABLE_LOOKAHEAD=$(DISABLE_LOOKAHEAD))
$(info DISABLE_INSN_CACHE_SIM=$(DISABLE_INSN_CACHE_SIM))
$(info DISABLE_DATA_CACHE_SIM=$(DISABLE_DATA_CACHE_SIM))

ifeq "$(DISABLE_LOOKAHEAD)" "yes"
BUILD_CONFIG_FLAGS +=-D'DISABLE_LOOKAHEAD'
endif

ifeq "$(DISABLE_INSN_CACHE_SIM)" "yes"
BUILD_CONFIG_FLAGS +=-D'DISABLE_INSN_CACHE_SIM'
endif

ifeq "$(DISABLE_DATA_CACHE_SIM)" "yes"
BUILD_CONFIG_FLAGS +=-D'DISABLE_DATA_CACHE_SIM'
endif

export BUILD_CONFIG_FLAGS

PHONY = all
all: clean build

PHONY += clean
clean: clean_core clean_utils clean_api clean_tracer clean_scripts

PHONY += build
build: build_llvm build_api build_intercept build_core build_tracer build_scripts

PHONY += build_intercept
build_intercept:
	@cd src/core/ld_preloading; $(MAKE) build

PHONY += build_core
build_core: build_intercept
	$(MAKE) -C $(KERNEL_SRC) M=$(SUBDIR)/build modules

PHONY += build_api_utils
build_api_utils:
	@cd src/api/utils; $(MAKE) build;

PHONY += build_api
build_api: build_api_utils
	@cd src/api; $(MAKE) build_api;

PHONY += build_tracer
build_tracer: build_api
	@cd src/tracer; $(MAKE) build;

PHONY += build_scripts
build_scripts:
	@cd scripts; $(MAKE) build;

PHONY += build_llvm
build_llvm:
	$(shell ./clang_install.sh)

PHONY += load
load:
	sudo insmod build/vt_module.ko

PHONY += unload
unload:
	sudo rmmod build/vt_module.ko

PHONY += clean_scripts
clean_scripts:
	@echo "Cleaning scripts ..."
	@cd scripts && $(MAKE) clean
	
PHONY += clean_core
clean_core:
	@echo "Cleaning old build files ..."
	@cd src/core/ld_preloading && $(MAKE) clean;
	@$(RM) -f build/*.ko build/*.o build/*.mod.c build/Module.symvers build/modules.order build/.*.cmd build/.cache.mk;
	@$(RM) -rf build/.tmp_versions;
	@$(RM) -f src/core/.*.cmd src/core/*.o;

PHONY += clean_utils
clean_utils:
	@echo "Cleaning old utils files ..."
	@$(RM) -f src/utils/*.o  src/utils/.*.cmd;	

PHONY += clean_api
clean_api:
	@echo "Cleaning old api files ..."
	@cd src/api/utils && $(MAKE) clean;
	@$(RM) -f src/api/*.o 	
	@cd src/api && $(MAKE) clean

PHONY += clean_tracer
clean_tracer:
	@echo "Cleaning Tracer files ..."
	@$(RM) -f src/tracer/*.o src/tracer/utils/*.o src/tracer/tests/*.o
	@cd src/tracer && $(MAKE) clean

.PHONY: $(PHONY)