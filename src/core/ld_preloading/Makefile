home_dir=$(HOME)
ldpreloading_dir=${home_dir}/Titan/src/core/ld_preloading
GCC:=gcc
RM:=rm

.PHONY: build	
build:
	$(info "VT-Intercept Lib BUILD_CONFIG_FLAGS=$(BUILD_CONFIG_FLAGS))
	@echo 'Building VT Intercept Library ...'
	@gcc ${ldpreloading_dir}/intercept_lib.c \
		${ldpreloading_dir}/vt_management.c \
		${ldpreloading_dir}/socket_utils.c -lsyscall_intercept \
		-fpic -shared -o libvtintercept.so $(BUILD_CONFIG_FLAGS)
	@mv libvtintercept.so /usr/lib
	@chmod 777 /usr/lib/libvtintercept.so

.PHONY: clean
clean: 
	@rm -f *.o > /dev/null
	@rm -f /usr/lib/libvtintercept.so > /dev/null
