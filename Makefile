EXTRA_CFLAGS += 
KERNEL_SRC:= /lib/modules/$(shell uname -r)/build
SUBDIR= $(shell pwd)
GCC:=gcc
RM:=rm

.PHONY : clean
nCpus=$(shell nproc --all)

all: clean build

clean: clean_core clean_utils clean_api clean_tracer clean_scripts

build: build_api build_intercept build_core build_tracer build_scripts

build_intercept:
	@cd src/core/ld_preloading; $(MAKE) build

build_core: build_intercept
	$(MAKE) -C $(KERNEL_SRC) M=$(SUBDIR)/build modules

build_api_utils:
	@cd src/api/utils; $(MAKE) build;

build_api: build_api_utils
	@cd src/api; $(MAKE) build_api;

build_tracer: build_api
	@cd src/tracer; $(MAKE) build;

build_scripts:
	@cd scripts; $(MAKE) build;

load:
	sudo insmod build/vt_module.ko

unload:
	sudo rmmod build/vt_module.ko


clean_scripts:
	@echo "Cleaning scripts ..."
	@cd scripts && $(MAKE) clean
	
clean_core:
	@echo "Cleaning old build files ..."
	@cd src/core/ld_preloading && $(MAKE) clean;
	@$(RM) -f build/*.ko build/*.o build/*.mod.c build/Module.symvers build/modules.order build/.*.cmd build/.cache.mk;
	@$(RM) -rf build/.tmp_versions;
	@$(RM) -f src/core/.*.cmd src/core/*.o;

clean_utils:
	@echo "Cleaning old utils files ..."
	@$(RM) -f src/utils/*.o  src/utils/.*.cmd;	

clean_api:
	@echo "Cleaning old api files ..."
	@cd src/api/utils && $(MAKE) clean;
	@$(RM) -f src/api/*.o 	
	@cd src/api && $(MAKE) clean

clean_tracer:
	@echo "Cleaning Tracer files ..."
	@$(RM) -f src/tracer/*.o src/tracer/utils/*.o src/tracer/tests/*.o
	@cd src/tracer && $(MAKE) clean
