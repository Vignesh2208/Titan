home_dir=$(HOME)
utils_dir=${home_dir}/Titan/src/api/utils

build:	
	@gcc -w -c -Wall -Werror -fpic ${utils_dir}/linkedlist.c \
	 ${utils_dir}/hashmap.c
	 @gcc -w -shared -o libvtutils.so ${utils_dir}/linkedlist.o \
	 ${utils_dir}/hashmap.o
	 @mv libvtutils.so /usr/lib
	 @chmod 0777 /usr/lib/libvtutils.so

	 @cp ${utils_dir}/linkedlist.h /usr/local/include
	 @cp ${utils_dir}/hashmap.h /usr/local/include

clean:
	@rm -f *.o
	@rm -f /usr/lib/libvtutils.so > /dev/null
	@rm -f /usr/local/include/linkedlist.h > /dev/null
	@rm -f /usr/local/include/hashmap.h > /dev/null
