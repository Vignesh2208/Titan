# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.18

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Disable VCS-based implicit rules.
% : %,v


# Disable VCS-based implicit rules.
% : RCS/%


# Disable VCS-based implicit rules.
% : RCS/%,v


# Disable VCS-based implicit rules.
% : SCCS/s.%


# Disable VCS-based implicit rules.
% : s.%


.SUFFIXES: .hpux_make_needs_suffix_list


# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/local/lib/python3.7/dist-packages/cmake/data/bin/cmake

# The command to remove a file.
RM = /usr/local/lib/python3.7/dist-packages/cmake/data/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/kronos/Titan/ld_preloading/syscall_intercept

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/kronos/Titan/ld_preloading/syscall_intercept/build

# Include any dependencies generated for this target.
include test/CMakeFiles/filter_test.dir/depend.make

# Include the progress variables for this target.
include test/CMakeFiles/filter_test.dir/progress.make

# Include the compile flags for this target's objects.
include test/CMakeFiles/filter_test.dir/flags.make

test/CMakeFiles/filter_test.dir/filter_test.c.o: test/CMakeFiles/filter_test.dir/flags.make
test/CMakeFiles/filter_test.dir/filter_test.c.o: ../test/filter_test.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/kronos/Titan/ld_preloading/syscall_intercept/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building C object test/CMakeFiles/filter_test.dir/filter_test.c.o"
	cd /home/kronos/Titan/ld_preloading/syscall_intercept/build/test && /usr/bin/gcc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/filter_test.dir/filter_test.c.o -c /home/kronos/Titan/ld_preloading/syscall_intercept/test/filter_test.c

test/CMakeFiles/filter_test.dir/filter_test.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/filter_test.dir/filter_test.c.i"
	cd /home/kronos/Titan/ld_preloading/syscall_intercept/build/test && /usr/bin/gcc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/kronos/Titan/ld_preloading/syscall_intercept/test/filter_test.c > CMakeFiles/filter_test.dir/filter_test.c.i

test/CMakeFiles/filter_test.dir/filter_test.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/filter_test.dir/filter_test.c.s"
	cd /home/kronos/Titan/ld_preloading/syscall_intercept/build/test && /usr/bin/gcc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/kronos/Titan/ld_preloading/syscall_intercept/test/filter_test.c -o CMakeFiles/filter_test.dir/filter_test.c.s

# Object files for target filter_test
filter_test_OBJECTS = \
"CMakeFiles/filter_test.dir/filter_test.c.o"

# External object files for target filter_test
filter_test_EXTERNAL_OBJECTS =

test/filter_test: test/CMakeFiles/filter_test.dir/filter_test.c.o
test/filter_test: test/CMakeFiles/filter_test.dir/build.make
test/filter_test: libsyscall_intercept.so.0.1.0
test/filter_test: test/CMakeFiles/filter_test.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/kronos/Titan/ld_preloading/syscall_intercept/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking C executable filter_test"
	cd /home/kronos/Titan/ld_preloading/syscall_intercept/build/test && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/filter_test.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
test/CMakeFiles/filter_test.dir/build: test/filter_test

.PHONY : test/CMakeFiles/filter_test.dir/build

test/CMakeFiles/filter_test.dir/clean:
	cd /home/kronos/Titan/ld_preloading/syscall_intercept/build/test && $(CMAKE_COMMAND) -P CMakeFiles/filter_test.dir/cmake_clean.cmake
.PHONY : test/CMakeFiles/filter_test.dir/clean

test/CMakeFiles/filter_test.dir/depend:
	cd /home/kronos/Titan/ld_preloading/syscall_intercept/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/kronos/Titan/ld_preloading/syscall_intercept /home/kronos/Titan/ld_preloading/syscall_intercept/test /home/kronos/Titan/ld_preloading/syscall_intercept/build /home/kronos/Titan/ld_preloading/syscall_intercept/build/test /home/kronos/Titan/ld_preloading/syscall_intercept/build/test/CMakeFiles/filter_test.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : test/CMakeFiles/filter_test.dir/depend

