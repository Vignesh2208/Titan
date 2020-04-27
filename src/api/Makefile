home_dir=$(HOME)
api_dir=${home_dir}/Titan/src/api

build_api:

	 @echo "Building VT-API shared library ..."
	 @gcc -w -c -Wall -Werror -fpic ${api_dir}/VT_functions.c \
	 ${api_dir}/utility_functions.c 
	 @gcc -w -shared -o libvtapi.so ${api_dir}/VT_functions.o \
	 ${api_dir}/utility_functions.o 
	 @mv libvtapi.so /usr/lib
	 @chmod 777 /usr/lib/libvtapi.so

	 @cp ${api_dir}/VT_functions.h /usr/local/include
	 @cp ${api_dir}/utility_functions.h /usr/local/include
	 

	 @echo "Building VTL-Logic shared library ..."
	 @gcc -w -c -Wall -Werror -fpic ${api_dir}/VT_functions.c \
	 ${api_dir}/utility_functions.c ${api_dir}/vtl_logic.c \
	 ${api_dir}/fd_handling.c \
	 ${api_dir}/utils/linkedlist.c ${api_dir}/utils/hashmap.c

	 @gcc -w -shared -o libvtllogic.so ${api_dir}/VT_functions.o \
	 ${api_dir}/utility_functions.o ${api_dir}/vtl_logic.o \
	 ${api_dir}/fd_handling.o \
	 ${api_dir}/utils/linkedlist.o ${api_dir}/utils/hashmap.o

	 @mv libvtllogic.so /usr/lib
	 @chmod 777 /usr/lib/libvtllogic.so

	 @python setup.py install

clean:

	@rm -rf build/
	@rm -f *.o
	@rm -f /usr/lib/libvtapi.so > /dev/null
	@rm -f /usr/lib/libvtllogic.so > /dev/null
	@rm -f /usr/local/include/VT_functions.h > /dev/null
	@rm -f /usr/local/include/utility_functions.h > /dev/null
