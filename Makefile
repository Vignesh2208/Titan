EXTRA_CFLAGS += 
KERNEL_SRC:= /lib/modules/$(shell uname -r)/build

TITAN_BUILD_DIR=$(HOME)/Titan/build
TRACER_DIR=$(HOME)/Titan/src/tracer
SCRIPTS_DIR=$(HOME)/Titan/scripts
CORE_DIR=$(HOME)/Titan/src/core
UTILS_DIR=$(HOME)/Titan/src/utils
INTERCEPT_LIB_DIR=$(HOME)/Titan/src/core/intercept_lib
API_DIR=$(HOME)/Titan/src/api
API_UTILS_DIR=$(HOME)/Titan/src/api/utils

GCC:=gcc
RM:=rm
CD:=cd
ECHO:=echo
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
build: build_api build_intercept build_core

PHONY += install
install: install_api install_intercept install_core install_tracer install_scripts

PHONY += build_intercept
build_intercept: build_api
	@$(CD) ${INTERCEPT_LIB_DIR}; $(MAKE) build

PHONY += build_core
build_core: build_intercept
	$(MAKE) -C $(KERNEL_SRC) M=$(TITAN_BUILD_DIR) modules

PHONY += build_api_utils
build_api_utils:
	@$(CD) ${API_UTILS_DIR}; $(MAKE) build;

PHONY += build_api
build_api: build_api_utils
	@$(CD) ${API_DIR}; $(MAKE) build_api;


PHONY += build_intercept
install_intercept:
	@$(CD) ${INTERCEPT_LIB_DIR}; sudo $(MAKE) install

PHONY += build_core
install_core:
	$(ECHO) "Loading Titan virtual time module ..."
	#sudo insmod ${TITAN_BUILD_DIR}/vt_module.ko

PHONY += install_api_utils
install_api_utils:
	@$(CD) ${API_UTILS_DIR}; sudo $(MAKE) install;

PHONY += install_api
install_api:
	@$(CD) ${API_DIR}; sudo $(MAKE) install;

PHONY += install_tracer
install_tracer:
	@$(CD) ${TRACER_DIR}; sudo $(MAKE) install;

PHONY += install_scripts
install_scripts:
	@$(CD) ${SCRIPTS_DIR}; sudo $(MAKE) install;



PHONY += load
load:
	sudo insmod ${TITAN_BUILD_DIR}/vt_module.ko

PHONY += unload
unload:
	sudo rmmod ${TITAN_BUILD_DIR}/vt_module.ko

PHONY += clean_scripts
clean_scripts:
	@$(ECHO) "Cleaning scripts ..."
	@$(CD) ${SCRIPTS_DIR} && $(MAKE) clean
	
PHONY += clean_core
clean_core:
	@$(ECHO) "Cleaning old build files ..."
	@$(ECHO) "Cleaning intercept lib ..."
	@$(CD) ${INTERCEPT_LIB_DIR} && $(MAKE) clean;
	@$(RM) -f ${TITAN_BUILD_DIR}/*.ko
	@$(RM) -f ${TITAN_BUILD_DIR}/*.o
	@$(RM) -f ${TITAN_BUILD_DIR}/*.mod.c
	@$(RM) -f ${TITAN_BUILD_DIR}/Module.symvers
	@$(RM) -f ${TITAN_BUILD_DIR}/modules.order
	@$(RM) -f ${TITAN_BUILD_DIR}/.*.cmd 
	@$(RM) -f ${TITAN_BUILD_DIR}/.cache.mk;
	@$(RM) -rf ${TITAN_BUILD_DIR}/.tmp_versions;
	@$(RM) -f ${CORE_DIR}/.*.cmd ${CORE_DIR}/*.o;

PHONY += clean_utils
clean_utils:
	@$(ECHO) "Cleaning old utils files ..."
	@$(RM) -f ${UTILS_DIR}/*.o  ${UTILS_DIR}/.*.cmd;	

PHONY += clean_api
clean_api:
	@$(ECHO) "Cleaning old api files ..."
	@$(CD) ${API_UTILS_DIR} && $(MAKE) clean;
	@$(RM) -f ${API_DIR}/*.o 	
	@$(CD) ${API_DIR} && $(MAKE) clean

PHONY += clean_tracer
clean_tracer:
	@$(ECHO) "Cleaning Tracer files ..."
	@$(RM) -f ${TRACER_DIR}/*.o
	@$(CD) ${TRACER_DIR} && $(MAKE) clean

.PHONY: $(PHONY)