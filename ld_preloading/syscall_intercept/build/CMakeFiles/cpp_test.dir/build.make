# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.10

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list


# Suppress display of executed commands.
$(VERBOSE).SILENT:


# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/titan/Titan/ld_preloading/syscall_intercept

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/titan/Titan/ld_preloading/syscall_intercept/build

# Include any dependencies generated for this target.
include CMakeFiles/cpp_test.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/cpp_test.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/cpp_test.dir/flags.make

CMakeFiles/cpp_test.dir/src/cpp_compile_test.cc.o: CMakeFiles/cpp_test.dir/flags.make
CMakeFiles/cpp_test.dir/src/cpp_compile_test.cc.o: ../src/cpp_compile_test.cc
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/titan/Titan/ld_preloading/syscall_intercept/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/cpp_test.dir/src/cpp_compile_test.cc.o"
	/usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/cpp_test.dir/src/cpp_compile_test.cc.o -c /home/titan/Titan/ld_preloading/syscall_intercept/src/cpp_compile_test.cc

CMakeFiles/cpp_test.dir/src/cpp_compile_test.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/cpp_test.dir/src/cpp_compile_test.cc.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/titan/Titan/ld_preloading/syscall_intercept/src/cpp_compile_test.cc > CMakeFiles/cpp_test.dir/src/cpp_compile_test.cc.i

CMakeFiles/cpp_test.dir/src/cpp_compile_test.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/cpp_test.dir/src/cpp_compile_test.cc.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/titan/Titan/ld_preloading/syscall_intercept/src/cpp_compile_test.cc -o CMakeFiles/cpp_test.dir/src/cpp_compile_test.cc.s

CMakeFiles/cpp_test.dir/src/cpp_compile_test.cc.o.requires:

.PHONY : CMakeFiles/cpp_test.dir/src/cpp_compile_test.cc.o.requires

CMakeFiles/cpp_test.dir/src/cpp_compile_test.cc.o.provides: CMakeFiles/cpp_test.dir/src/cpp_compile_test.cc.o.requires
	$(MAKE) -f CMakeFiles/cpp_test.dir/build.make CMakeFiles/cpp_test.dir/src/cpp_compile_test.cc.o.provides.build
.PHONY : CMakeFiles/cpp_test.dir/src/cpp_compile_test.cc.o.provides

CMakeFiles/cpp_test.dir/src/cpp_compile_test.cc.o.provides.build: CMakeFiles/cpp_test.dir/src/cpp_compile_test.cc.o


CMakeFiles/cpp_test.dir/src/cpp_compile_mock.c.o: CMakeFiles/cpp_test.dir/flags.make
CMakeFiles/cpp_test.dir/src/cpp_compile_mock.c.o: ../src/cpp_compile_mock.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/titan/Titan/ld_preloading/syscall_intercept/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building C object CMakeFiles/cpp_test.dir/src/cpp_compile_mock.c.o"
	/usr/bin/gcc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/cpp_test.dir/src/cpp_compile_mock.c.o   -c /home/titan/Titan/ld_preloading/syscall_intercept/src/cpp_compile_mock.c

CMakeFiles/cpp_test.dir/src/cpp_compile_mock.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/cpp_test.dir/src/cpp_compile_mock.c.i"
	/usr/bin/gcc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/titan/Titan/ld_preloading/syscall_intercept/src/cpp_compile_mock.c > CMakeFiles/cpp_test.dir/src/cpp_compile_mock.c.i

CMakeFiles/cpp_test.dir/src/cpp_compile_mock.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/cpp_test.dir/src/cpp_compile_mock.c.s"
	/usr/bin/gcc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/titan/Titan/ld_preloading/syscall_intercept/src/cpp_compile_mock.c -o CMakeFiles/cpp_test.dir/src/cpp_compile_mock.c.s

CMakeFiles/cpp_test.dir/src/cpp_compile_mock.c.o.requires:

.PHONY : CMakeFiles/cpp_test.dir/src/cpp_compile_mock.c.o.requires

CMakeFiles/cpp_test.dir/src/cpp_compile_mock.c.o.provides: CMakeFiles/cpp_test.dir/src/cpp_compile_mock.c.o.requires
	$(MAKE) -f CMakeFiles/cpp_test.dir/build.make CMakeFiles/cpp_test.dir/src/cpp_compile_mock.c.o.provides.build
.PHONY : CMakeFiles/cpp_test.dir/src/cpp_compile_mock.c.o.provides

CMakeFiles/cpp_test.dir/src/cpp_compile_mock.c.o.provides.build: CMakeFiles/cpp_test.dir/src/cpp_compile_mock.c.o


# Object files for target cpp_test
cpp_test_OBJECTS = \
"CMakeFiles/cpp_test.dir/src/cpp_compile_test.cc.o" \
"CMakeFiles/cpp_test.dir/src/cpp_compile_mock.c.o"

# External object files for target cpp_test
cpp_test_EXTERNAL_OBJECTS =

cpp_test: CMakeFiles/cpp_test.dir/src/cpp_compile_test.cc.o
cpp_test: CMakeFiles/cpp_test.dir/src/cpp_compile_mock.c.o
cpp_test: CMakeFiles/cpp_test.dir/build.make
cpp_test: CMakeFiles/cpp_test.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/titan/Titan/ld_preloading/syscall_intercept/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Linking CXX executable cpp_test"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/cpp_test.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/cpp_test.dir/build: cpp_test

.PHONY : CMakeFiles/cpp_test.dir/build

CMakeFiles/cpp_test.dir/requires: CMakeFiles/cpp_test.dir/src/cpp_compile_test.cc.o.requires
CMakeFiles/cpp_test.dir/requires: CMakeFiles/cpp_test.dir/src/cpp_compile_mock.c.o.requires

.PHONY : CMakeFiles/cpp_test.dir/requires

CMakeFiles/cpp_test.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/cpp_test.dir/cmake_clean.cmake
.PHONY : CMakeFiles/cpp_test.dir/clean

CMakeFiles/cpp_test.dir/depend:
	cd /home/titan/Titan/ld_preloading/syscall_intercept/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/titan/Titan/ld_preloading/syscall_intercept /home/titan/Titan/ld_preloading/syscall_intercept /home/titan/Titan/ld_preloading/syscall_intercept/build /home/titan/Titan/ld_preloading/syscall_intercept/build /home/titan/Titan/ld_preloading/syscall_intercept/build/CMakeFiles/cpp_test.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/cpp_test.dir/depend

