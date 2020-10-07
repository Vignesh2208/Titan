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
include test/CMakeFiles/asm_pattern.dir/depend.make

# Include the progress variables for this target.
include test/CMakeFiles/asm_pattern.dir/progress.make

# Include the compile flags for this target's objects.
include test/CMakeFiles/asm_pattern.dir/flags.make

test/CMakeFiles/asm_pattern.dir/asm_pattern.c.o: test/CMakeFiles/asm_pattern.dir/flags.make
test/CMakeFiles/asm_pattern.dir/asm_pattern.c.o: ../test/asm_pattern.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/kronos/Titan/ld_preloading/syscall_intercept/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building C object test/CMakeFiles/asm_pattern.dir/asm_pattern.c.o"
	cd /home/kronos/Titan/ld_preloading/syscall_intercept/build/test && /usr/bin/gcc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/asm_pattern.dir/asm_pattern.c.o -c /home/kronos/Titan/ld_preloading/syscall_intercept/test/asm_pattern.c

test/CMakeFiles/asm_pattern.dir/asm_pattern.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/asm_pattern.dir/asm_pattern.c.i"
	cd /home/kronos/Titan/ld_preloading/syscall_intercept/build/test && /usr/bin/gcc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/kronos/Titan/ld_preloading/syscall_intercept/test/asm_pattern.c > CMakeFiles/asm_pattern.dir/asm_pattern.c.i

test/CMakeFiles/asm_pattern.dir/asm_pattern.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/asm_pattern.dir/asm_pattern.c.s"
	cd /home/kronos/Titan/ld_preloading/syscall_intercept/build/test && /usr/bin/gcc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/kronos/Titan/ld_preloading/syscall_intercept/test/asm_pattern.c -o CMakeFiles/asm_pattern.dir/asm_pattern.c.s

# Object files for target asm_pattern
asm_pattern_OBJECTS = \
"CMakeFiles/asm_pattern.dir/asm_pattern.c.o"

# External object files for target asm_pattern
asm_pattern_EXTERNAL_OBJECTS = \
"/home/kronos/Titan/ld_preloading/syscall_intercept/build/CMakeFiles/syscall_intercept_base_c.dir/src/disasm_wrapper.c.o" \
"/home/kronos/Titan/ld_preloading/syscall_intercept/build/CMakeFiles/syscall_intercept_base_c.dir/src/intercept.c.o" \
"/home/kronos/Titan/ld_preloading/syscall_intercept/build/CMakeFiles/syscall_intercept_base_c.dir/src/intercept_desc.c.o" \
"/home/kronos/Titan/ld_preloading/syscall_intercept/build/CMakeFiles/syscall_intercept_base_c.dir/src/intercept_log.c.o" \
"/home/kronos/Titan/ld_preloading/syscall_intercept/build/CMakeFiles/syscall_intercept_base_c.dir/src/intercept_util.c.o" \
"/home/kronos/Titan/ld_preloading/syscall_intercept/build/CMakeFiles/syscall_intercept_base_c.dir/src/patcher.c.o" \
"/home/kronos/Titan/ld_preloading/syscall_intercept/build/CMakeFiles/syscall_intercept_base_c.dir/src/magic_syscalls.c.o" \
"/home/kronos/Titan/ld_preloading/syscall_intercept/build/CMakeFiles/syscall_intercept_base_c.dir/src/syscall_formats.c.o" \
"/home/kronos/Titan/ld_preloading/syscall_intercept/build/CMakeFiles/syscall_intercept_base_asm.dir/src/intercept_template.S.o" \
"/home/kronos/Titan/ld_preloading/syscall_intercept/build/CMakeFiles/syscall_intercept_base_asm.dir/src/util.S.o" \
"/home/kronos/Titan/ld_preloading/syscall_intercept/build/CMakeFiles/syscall_intercept_base_asm.dir/src/intercept_wrapper.S.o"

test/asm_pattern: test/CMakeFiles/asm_pattern.dir/asm_pattern.c.o
test/asm_pattern: CMakeFiles/syscall_intercept_base_c.dir/src/disasm_wrapper.c.o
test/asm_pattern: CMakeFiles/syscall_intercept_base_c.dir/src/intercept.c.o
test/asm_pattern: CMakeFiles/syscall_intercept_base_c.dir/src/intercept_desc.c.o
test/asm_pattern: CMakeFiles/syscall_intercept_base_c.dir/src/intercept_log.c.o
test/asm_pattern: CMakeFiles/syscall_intercept_base_c.dir/src/intercept_util.c.o
test/asm_pattern: CMakeFiles/syscall_intercept_base_c.dir/src/patcher.c.o
test/asm_pattern: CMakeFiles/syscall_intercept_base_c.dir/src/magic_syscalls.c.o
test/asm_pattern: CMakeFiles/syscall_intercept_base_c.dir/src/syscall_formats.c.o
test/asm_pattern: CMakeFiles/syscall_intercept_base_asm.dir/src/intercept_template.S.o
test/asm_pattern: CMakeFiles/syscall_intercept_base_asm.dir/src/util.S.o
test/asm_pattern: CMakeFiles/syscall_intercept_base_asm.dir/src/intercept_wrapper.S.o
test/asm_pattern: test/CMakeFiles/asm_pattern.dir/build.make
test/asm_pattern: test/CMakeFiles/asm_pattern.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/kronos/Titan/ld_preloading/syscall_intercept/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking C executable asm_pattern"
	cd /home/kronos/Titan/ld_preloading/syscall_intercept/build/test && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/asm_pattern.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
test/CMakeFiles/asm_pattern.dir/build: test/asm_pattern

.PHONY : test/CMakeFiles/asm_pattern.dir/build

test/CMakeFiles/asm_pattern.dir/clean:
	cd /home/kronos/Titan/ld_preloading/syscall_intercept/build/test && $(CMAKE_COMMAND) -P CMakeFiles/asm_pattern.dir/cmake_clean.cmake
.PHONY : test/CMakeFiles/asm_pattern.dir/clean

test/CMakeFiles/asm_pattern.dir/depend:
	cd /home/kronos/Titan/ld_preloading/syscall_intercept/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/kronos/Titan/ld_preloading/syscall_intercept /home/kronos/Titan/ld_preloading/syscall_intercept/test /home/kronos/Titan/ld_preloading/syscall_intercept/build /home/kronos/Titan/ld_preloading/syscall_intercept/build/test /home/kronos/Titan/ld_preloading/syscall_intercept/build/test/CMakeFiles/asm_pattern.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : test/CMakeFiles/asm_pattern.dir/depend

