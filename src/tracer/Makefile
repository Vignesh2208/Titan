
home_dir=$(HOME)
tracer_dir=${home_dir}/Titan/src/tracer
EXTRA_CFLAGS += 
KERNEL_SRC:= /lib/modules/$(shell uname -r)/build
SUBDIR= $(shell pwd)
GCC:=gcc
RM:=rm

libs:
	@cd ${tracer_dir}/libperf  && ./configure  && $(MAKE) clean && $(MAKE) && $(MAKE) install
	

build_tracer:
	@gcc -w -I/usr/local/include/perf -I/usr/local/lib/perf/include -I${tracer_dir}/utils \
	 ${tracer_dir}/tracer.c ${tracer_dir}/utils.c ${tracer_dir}/utils/linkedlist.c \
	 ${tracer_dir}/utils/hashmap.c -o tracer -lperf -lvtapi
	@chmod 777 tracer
	@mv tracer /usr/bin
	@gcc ${tracer_dir}/app_vt_test_tracer.c -o app_vt_test_tracer -lpthread -lvtapi
	@mv app_vt_test_tracer /usr/bin
	@gcc ${tracer_dir}/app_vt_tracer.c -o app_vt_tracer -lpthread -lvtapi
	@mv app_vt_tracer /usr/bin
	@cp /usr/local/lib/libperf.so.0 /usr/lib/
	@cd ${tracer_dir}/utils && $(MAKE) build

build_tracer_debug:
	@gcc -w -I/usr/local/include/perf -I/usr/local/lib/perf/include -I${tracer_dir}/utils \
	-D DEBUG ${tracer_dir}/tracer.c ${tracer_dir}/utils.c ${tracer_dir}/utils/linkedlist.c \
	${tracer_dir}/utils/hashmap.c -o tracer -lperf -lvtapi
	@gcc ${tracer_dir}/app_vt_test_tracer.c -o app_vt_test_tracer -lpthread -lvtapi
	@mv app_vt_test_tracer /usr/bin
	@gcc ${tracer_dir}/app_vt_tracer.c -o app_vt_tracer -lpthread -lvtapi
	@mv app_vt_tracer /usr/bin
	@chmod 777 tracer
	@mv tracer /usr/bin
	@cp /usr/local/lib/libperf.so.0 /usr/lib/
	@cd ${tracer_dir}/utils && $(MAKE) build


clean: 
	@cd ${tracer_dir}/libperf && ./configure && $(MAKE) clean
	@cd ${tracer_dir}/tests && $(MAKE) clean
	@rm -f *.o > /dev/null
	@rm -f /usr/bin/tracer > /dev/null
	@rm -f /usr/bin/tracer_test > /dev/null
	@rm -f /usr/bin/app_vt_test_tracer > /dev/null
	@rm -f /usr/bin/app_vt_tracer > /dev/null
	@cd ${tracer_dir}/utils && $(MAKE) clean

build_tests:
	@cd ${tracer_dir}/tests && $(MAKE) build
	@cd ${tracer_dir}/utils && $(MAKE) build

build: libs build_tracer build_tests

debug: libs build_tracer_test build_tracer_debug build_tests








