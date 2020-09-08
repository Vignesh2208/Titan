SCRIPTS_DIR=$(HOME)/Titan/scripts

GCC:=gcc
CLANG:=clang
CP:=cp
RM:=rm
MKDIR:=mkdir
CHMOD:=chmod

.PHONY: install
install: 
	@$(MKDIR) -p ${SCRIPTS_DIR}/bin
	@$(CLANG) -O1 -o ${SCRIPTS_DIR}/bin/timerfd_example -lvtllogic timerfd_example.c
	@$(GCC) -o ${SCRIPTS_DIR}/bin/x64_synchronizer x64_synchronizer.c
	@$(CLANG) -O1 -o ${SCRIPTS_DIR}/bin/x86_vt_spinner -lvtllogic x86_vt_spinner.c 
	@$(CLANG) -O1 -o ${SCRIPTS_DIR}/bin/tcp_server -lvtllogic tcp_server.c
	@$(CLANG) -O1 -o ${SCRIPTS_DIR}/bin/tcp_client -lvtllogic tcp_client.c
	@$(CLANG) -O1 -lvtllogic -o ${SCRIPTS_DIR}/bin/loop_example loop_example.c #-O1 flag here is crucial to enable loop optimization passes and loop set lookahead
	@$(CHMOD) -R 777 ${SCRIPTS_DIR}/bin/*
	@$(CP) ${SCRIPTS_DIR}/bin/x64_synchronizer /bin/
	@$(CP) ${SCRIPTS_DIR}/bin/x86_vt_spinner /bin/

.PHONY: clean
clean:
	@$(RM) -rf ${SCRIPTS_DIR}/bin
	@$(RM) -f *.bc
	@$(RM) -f *.ll
	@$(RM) -f *.artifacts.json
	@$(RM) -f *.s
	@$(RM) -f /bin/x64_synchronizer > /dev/null
	@$(RM) -f /bin/x86_vt_spinner > /dev/null
