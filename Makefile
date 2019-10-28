EXTRA_CFLAGS += 
KERNEL_SRC:= /lib/modules/$(shell uname -r)/build
SUBDIR= $(shell pwd)
GCC:=gcc
RM:=rm

.PHONY : clean
nCpus=$(shell nproc --all)

all: clean build

clean: clean_core clean_utils clean_api clean_tracer

build: build_core build_api build_tracer

setup_kernel: download_4_4_kernel compile_4_4_kernel

patch_kernel: update_kernel compile_4_4_kernel

update_kernel:
	@cd src/kernel_changes/linux-4.4.5 && ./patch_kernel.sh

download_4_4_kernel:
	@cd src/kernel_changes/linux-4.4.5 && ./setup.sh

compile_4_4_kernel:
	@cd /src/linux-4.4.5 && sudo cp -v /boot/config-`uname -r` .config && $(MAKE) menuconfig && $(MAKE) -j$(nCpus) && $(MAKE) modules_install && $(MAKE) install;

build_core: 
	$(MAKE) -C $(KERNEL_SRC) M=$(SUBDIR)/build modules


build_api:
	@cd src/api; $(MAKE) build_api;

build_tracer:
	@cd src/tracer; $(MAKE) build;


load:
	sudo insmod build/vt_module.ko

unload:
	sudo rmmod build/vt_module.ko


clean_tests:
	@echo "Cleaning test files ..."
	@cd tests && $(MAKE) clean

clean_core:
	@echo "Cleaning old build files ..."
	@$(RM) -f build/*.ko build/*.o build/*.mod.c build/Module.symvers build/modules.order ;

clean_utils:
	@echo "Cleaning old utils files ..."
	@$(RM) -f src/utils/*.o 	

clean_api:
	@echo "Cleaning old api files ..."
	@$(RM) -f src/api/*.o 	
	@cd src/api && $(MAKE) clean

clean_tracer:
	@echo "Cleaning Tracer files ..."
	@$(RM) -f src/tracer/*.o src/tracer/utils/*.o src/tracer/tests/*.o
	@cd src/tracer && $(MAKE) clean
