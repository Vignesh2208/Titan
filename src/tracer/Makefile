TRACER_DIR=$(HOME)/Titan/src/tracer

GCC:=gcc
RM:=rm
CP:=cp
CD:=cd
MKDIR:=mkdir
CHMOD:=chmod
ECHO:=echo

.PHONY: install
install:
	@$(ECHO) "Intalling tracer ..." 
	$(info "Tracer: BUILD_CONFIG_FLAGS=$(BUILD_CONFIG_FLAGS)")
	@$(MKDIR) -p ${TRACER_DIR}/bin
	@$(CC) ${TRACER_DIR}/tracer.c ${TRACER_DIR}/cJSON/cJSON.c \
		-o ${TRACER_DIR}/bin/ttn_tracer -lpthread -lvtapi $(BUILD_CONFIG_FLAGS)
	@$(CHMOD) 777 ${TRACER_DIR}/bin/ttn_tracer
	@$(CP) ${TRACER_DIR}/bin/ttn_tracer /usr/bin > /dev/null

.PHONY: clean
clean: 
	@$(RM) -f *.o > /dev/null
	@$(RM) -rf ${TRACER_DIR}/bin > /dev/null
	@$(RM) -f /usr/bin/ttn_tracer > /dev/null







