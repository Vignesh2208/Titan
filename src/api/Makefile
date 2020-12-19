API_DIR=$(HOME)/Titan/src/api

CC:=gcc
RM:=rm
CP:=cp
ECHO:=echo
CHMOD:=chmod
MKDIR:=mkdir

.PHONY: build_api
build_api:
	@$(ECHO) "Building VT-API shared library ..."
	@$(MKDIR) -p ${API_DIR}/build/include
	@$(MKDIR) -p ${API_DIR}/build/lib

	@$(CC) -w -c -g -Wall -Werror -fpic ${API_DIR}/VT_functions.c \
	${API_DIR}/utility_functions.c 
	@$(CC) -w -shared -o ${API_DIR}/build/lib/libvtapi.so \
	${API_DIR}/VT_functions.o ${API_DIR}/utility_functions.o 
	@$(CHMOD) 777 ${API_DIR}/build/lib/libvtapi.so

	@$(CP) ${API_DIR}/VT_functions.h ${API_DIR}/build/include
	@$(CP) ${API_DIR}/utility_functions.h ${API_DIR}/build/include


	$(info "VTL Logic: BUILD_CONFIG_FLAGS=$(BUILD_CONFIG_FLAGS)")
	@$(ECHO) "Building VTL-Logic shared library ..."

	@$(CC) -w -c -Wall -Werror -g -fpic ${API_DIR}/VT_functions.c \
	${API_DIR}/utility_functions.c ${API_DIR}/vtl_logic.c \
	${API_DIR}/fd_handling.c ${API_DIR}/lookahead_parsing.c \
	${API_DIR}/utils/linkedlist.c ${API_DIR}/cache_simulation.c \
	${API_DIR}/utils/hashmap.c \
	${API_DIR}/socket_layer/inet.c \
	${API_DIR}/socket_layer/ip_input.c \
	${API_DIR}/socket_layer/ip_output.c \
	${API_DIR}/socket_layer/tcp.c \
	${API_DIR}/socket_layer/tcp_input.c \
	${API_DIR}/socket_layer/tcp_output.c \
	${API_DIR}/socket_layer/tcp_data.c \
	${API_DIR}/socket_layer/socket.c \
	${API_DIR}/socket_layer/sock.c \
	${API_DIR}/socket_layer/netdev.c \
	${API_DIR}/socket_layer/utils.c \
	${API_DIR}/socket_layer/skbuff.c \
	${API_DIR}/socket_layer/timer.c \
	${API_DIR}/mpi_lib/vtmpi.c \
	$(BUILD_CONFIG_FLAGS)

	@$(CC) -g -w -shared -o ${API_DIR}/build/lib/libvtllogic.so \
	${API_DIR}/VT_functions.o ${API_DIR}/utility_functions.o \
	${API_DIR}/vtl_logic.o ${API_DIR}/fd_handling.o  \
	${API_DIR}/lookahead_parsing.o ${API_DIR}/cache_simulation.o \
	${API_DIR}/utils/linkedlist.o ${API_DIR}/utils/hashmap.o \
	${API_DIR}/inet.o \
	${API_DIR}/ip_input.o \
	${API_DIR}/ip_output.o \
	${API_DIR}/tcp.o \
	${API_DIR}/tcp_input.o \
	${API_DIR}/tcp_output.o \
	${API_DIR}/tcp_data.o \
	${API_DIR}/socket.o \
	${API_DIR}/sock.o \
	${API_DIR}/netdev.o \
	${API_DIR}/utils.o \
	${API_DIR}/skbuff.o \
	${API_DIR}/timer.o \
	${API_DIR}/vtmpi.o \
	-lpthread	

	@$(CHMOD) 777 ${API_DIR}/build/lib/libvtllogic.so
	@$(CHMOD) +x setup.py

.PHONY: install
install:
	./setup.py install
	@$(CP) ${API_DIR}/build/lib/libvtllogic.so /usr/lib > /dev/null
	@$(CP) ${API_DIR}/build/lib/libvtapi.so /usr/lib > /dev/null
	@$(CP) ${API_DIR}/VT_functions.h /usr/local/include > /dev/null
	@$(CP) ${API_DIR}/mpi_lib/vtmpi.h /usr/local/include > /dev/null
	@$(CP) ${API_DIR}/utility_functions.h /usr/local/include > /dev/null

.PHONY: clean
clean:
	@$(RM) -f ${API_DIR}/build/lib/libvtllogic.so > /dev/null
	@$(RM) -f ${API_DIR}/build/lib/libvtapi.so > /dev/null
	@$(RM) -f /usr/lib/libvtapi.so > /dev/null
	@$(RM) -f /usr/lib/libvtllogic.so > /dev/null
	@$(RM) -f /usr/local/include/VT_functions.h > /dev/null
	@$(RM) -f /usr/local/include/utility_functions.h > /dev/null
	@$(RM) -f /usr/local/include/vtmpi.h > /dev/null
	@$(RM) -rf ${API_DIR}/build
	@$(RM) -f *.o
