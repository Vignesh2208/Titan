INTERCEPT_LIB_DIR=$(HOME)/Titan/src/core/intercept_lib

GCC:=gcc
CP:=cp
RM:=rm
MKDIR:=mkdir
CHMOD:=chmod
ECHO:=echo

.PHONY: build	
build:

	@$(MKDIR) -p ${INTERCEPT_LIB_DIR}/build/lib

	$(info "VT-Intercept Lib BUILD_CONFIG_FLAGS=$(BUILD_CONFIG_FLAGS))
	@$(ECHO) 'Building VT Intercept Library ...'
	@$(GCC) -I$(HOME)/Titan/src/api/build/include \
			${INTERCEPT_LIB_DIR}/intercept_lib.c \
			${INTERCEPT_LIB_DIR}/vt_management.c \
			${INTERCEPT_LIB_DIR}/socket_utils.c \
			-lsyscall_intercept -lpthread \
			-fpic -shared -o ${INTERCEPT_LIB_DIR}/build/lib/libvtintercept.so $(BUILD_CONFIG_FLAGS)
	@$(CHMOD) 777 ${INTERCEPT_LIB_DIR}/build/lib/libvtintercept.so

.PHONY: install
install:
	@$(CP) ${INTERCEPT_LIB_DIR}/build/lib/libvtintercept.so /usr/lib > /dev/null

.PHONY: clean
clean: 
	@$(RM) -f /usr/lib/libvtintercept.so > /dev/null
	@$(RM) -rf ${INTERCEPT_LIB_DIR}/build > /dev/null
	@$(RM) -f *.o > /dev/null
