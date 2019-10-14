EXTRA_CFLAGS += 
KERNEL_SRC:= /lib/modules/$(shell uname -r)/build
SUBDIR= $(PWD)
KBUILD_CFLAGS += -w
GCC:=gcc
RM:=rm

.PHONY : clean

all: clean modules

obj-m:= test_module.o
test_module-objs := ../src/core/test_module.o

modules:
	$(MAKE) -C $(KERNEL_SRC) M=$(SUBDIR)/build modules 

clean:
	$(RM) -f *.ko *.o *.mod.c Module.symvers .*test_module* > /dev/null
	$(RM) -rf .tmp_versions
