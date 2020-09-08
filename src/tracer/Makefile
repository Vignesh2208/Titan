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
	$(info "Tracer install: BUILD_CONFIG_FLAGS=$(BUILD_CONFIG_FLAGS))
	@$(MKDIR) -p ${TRACER_DIR}/bin
	@$(CC) ${TRACER_DIR}/tracer.c ${TRACER_DIR}/cJSON/cJSON.c \
		-o ${TRACER_DIR}/bin/tracer -lpthread -lvtapi $(BUILD_CONFIG_FLAGS)
	@$(CHMOD) 777 ${TRACER_DIR}/bin/tracer
	@$(CP) ${TRACER_DIR}/bin/tracer /usr/bin > /dev/null

.PHONY: clean
clean: 
	@$(RM) -f *.o > /dev/null
	@$(RM) -rf ${TRACER_DIR}/bin > /dev/null
	@$(RM) -f /usr/bin/tracer > /dev/null







