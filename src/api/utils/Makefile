API_UTILS_DIR=$(HOME)/Titan/src/api/utils

GCC:=gcc
RM:=rm
CP:=cp
MKDIR:=mkdir
CHMOD:=chmod

.PHONY: build
build:
	@$(MKDIR) -p ${API_UTILS_DIR}/build/lib
	@$(CC) -w -c -Wall -Werror -fpic ${API_UTILS_DIR}/linkedlist.c \
	 ${API_UTILS_DIR}/hashmap.c
	@$(CC) -w -shared -o ${API_UTILS_DIR}/build/lib/libvtutils.so ${API_UTILS_DIR}/linkedlist.o \
	 ${API_UTILS_DIR}/hashmap.o
	@$(CHMOD) 777 ${API_UTILS_DIR}/build/lib/libvtutils.so

.PHONY: install
install:
	@$(CP) ${API_UTILS_DIR}/build/lib/libvtutils.so /usr/lib > /dev/null
	@$(CP) ${API_UTILS_DIR}/linkedlist.h /usr/local/include
	@$(CP) ${API_UTILS_DIR}/hashmap.h /usr/local/include

.PHONY: clean
clean:
	@$(RM) -rf ${API_UTILS_DIR}/build > /dev/null
	@$(RM) -f *.o > /dev/null
	@$(RM) -f /usr/lib/libvtutils.so > /dev/null
	@$(RM) -f /usr/local/include/linkedlist.h > /dev/null
	@$(RM) -f /usr/local/include/hashmap.h > /dev/null
