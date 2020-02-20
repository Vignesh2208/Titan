
home_dir=$(HOME)
tracer_dir=${home_dir}/Titan/src/tracer
EXTRA_CFLAGS += 
KERNEL_SRC:= /lib/modules/$(shell uname -r)/build
SUBDIR= $(shell pwd)
GCC:=gcc
RM:=rm

	
build_tracer:
	@gcc ${tracer_dir}/tracer.c -o tracer -lpthread -lvtapi
	@mv tracer /usr/bin
	
clean: 
	@rm -f *.o > /dev/null
	@rm -f /usr/bin/tracer > /dev/null
	
build: build_tracer 








