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
include CMakeFiles/syscall_intercept_base_asm.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/syscall_intercept_base_asm.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/syscall_intercept_base_asm.dir/flags.make

CMakeFiles/syscall_intercept_base_asm.dir/src/intercept_template.S.o: CMakeFiles/syscall_intercept_base_asm.dir/flags.make
CMakeFiles/syscall_intercept_base_asm.dir/src/intercept_template.S.o: ../src/intercept_template.S
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/kronos/Titan/ld_preloading/syscall_intercept/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building ASM object CMakeFiles/syscall_intercept_base_asm.dir/src/intercept_template.S.o"
	/usr/bin/gcc $(ASM_DEFINES) $(ASM_INCLUDES) $(ASM_FLAGS) -o CMakeFiles/syscall_intercept_base_asm.dir/src/intercept_template.S.o -c /home/kronos/Titan/ld_preloading/syscall_intercept/src/intercept_template.S

CMakeFiles/syscall_intercept_base_asm.dir/src/util.S.o: CMakeFiles/syscall_intercept_base_asm.dir/flags.make
CMakeFiles/syscall_intercept_base_asm.dir/src/util.S.o: ../src/util.S
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/kronos/Titan/ld_preloading/syscall_intercept/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building ASM object CMakeFiles/syscall_intercept_base_asm.dir/src/util.S.o"
	/usr/bin/gcc $(ASM_DEFINES) $(ASM_INCLUDES) $(ASM_FLAGS) -o CMakeFiles/syscall_intercept_base_asm.dir/src/util.S.o -c /home/kronos/Titan/ld_preloading/syscall_intercept/src/util.S

CMakeFiles/syscall_intercept_base_asm.dir/src/intercept_wrapper.S.o: CMakeFiles/syscall_intercept_base_asm.dir/flags.make
CMakeFiles/syscall_intercept_base_asm.dir/src/intercept_wrapper.S.o: ../src/intercept_wrapper.S
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/kronos/Titan/ld_preloading/syscall_intercept/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Building ASM object CMakeFiles/syscall_intercept_base_asm.dir/src/intercept_wrapper.S.o"
	/usr/bin/gcc $(ASM_DEFINES) $(ASM_INCLUDES) $(ASM_FLAGS) -o CMakeFiles/syscall_intercept_base_asm.dir/src/intercept_wrapper.S.o -c /home/kronos/Titan/ld_preloading/syscall_intercept/src/intercept_wrapper.S

syscall_intercept_base_asm: CMakeFiles/syscall_intercept_base_asm.dir/src/intercept_template.S.o
syscall_intercept_base_asm: CMakeFiles/syscall_intercept_base_asm.dir/src/util.S.o
syscall_intercept_base_asm: CMakeFiles/syscall_intercept_base_asm.dir/src/intercept_wrapper.S.o
syscall_intercept_base_asm: CMakeFiles/syscall_intercept_base_asm.dir/build.make

.PHONY : syscall_intercept_base_asm

# Rule to build all files generated by this target.
CMakeFiles/syscall_intercept_base_asm.dir/build: syscall_intercept_base_asm

.PHONY : CMakeFiles/syscall_intercept_base_asm.dir/build

CMakeFiles/syscall_intercept_base_asm.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/syscall_intercept_base_asm.dir/cmake_clean.cmake
.PHONY : CMakeFiles/syscall_intercept_base_asm.dir/clean

CMakeFiles/syscall_intercept_base_asm.dir/depend:
	cd /home/kronos/Titan/ld_preloading/syscall_intercept/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/kronos/Titan/ld_preloading/syscall_intercept /home/kronos/Titan/ld_preloading/syscall_intercept /home/kronos/Titan/ld_preloading/syscall_intercept/build /home/kronos/Titan/ld_preloading/syscall_intercept/build /home/kronos/Titan/ld_preloading/syscall_intercept/build/CMakeFiles/syscall_intercept_base_asm.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/syscall_intercept_base_asm.dir/depend

