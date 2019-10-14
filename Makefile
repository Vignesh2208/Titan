EXTRA_CFLAGS += 
KERNEL_SRC:= /lib/modules/$(shell uname -r)/build
SUBDIR= $(shell pwd)
GCC:=gcc
RM:=rm

.PHONY : clean
nCpus=$(shell nproc --all)

all: clean build

clean: clean_core clean_tests

build: build_core build_tests

build_tests: 
	@cd tests && $(MAKE) build

build_core: 
	$(MAKE) -C $(KERNEL_SRC) M=$(SUBDIR)/build modules

clean_tests:
	@echo "Cleaning test files ..."
	@cd tests && $(MAKE) clean

clean_core:
	@echo "Cleaning old build files ..."
	@$(RM) -f build/*.ko build/*.o build/*.mod.c build/Module.symvers build/modules.order ;
