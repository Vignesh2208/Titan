home_dir=$(HOME)
ldpreloading_dir=${home_dir}/Titan/src/core/ld_preloading
GCC:=gcc
RM:=rm

	
build:
	@gcc ${ldpreloading_dir}/intercept_lib.c \
		${ldpreloading_dir}/vt_management.c -lsyscall_intercept \
		-fpic -shared -o libvtintercept.so
	@mv libvtintercept.so /usr/lib
	
clean: 
	@rm -f *.o > /dev/null
	@rm -f /usr/lib/libvtintercept.so > /dev/null