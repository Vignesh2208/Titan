# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.16

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
CMAKE_COMMAND = /usr/local/bin/cmake

# The command to remove a file.
RM = /usr/local/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/titan/Titan/ld_preloading/syscall_intercept

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/titan/Titan/ld_preloading/syscall_intercept/build

# Include any dependencies generated for this target.
include test/CMakeFiles/pattern3.out.dir/depend.make

# Include the progress variables for this target.
include test/CMakeFiles/pattern3.out.dir/progress.make

# Include the compile flags for this target's objects.
include test/CMakeFiles/pattern3.out.dir/flags.make

test/CMakeFiles/pattern3.out.dir/pattern3.out.S.o: test/CMakeFiles/pattern3.out.dir/flags.make
test/CMakeFiles/pattern3.out.dir/pattern3.out.S.o: ../test/pattern3.out.S
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/titan/Titan/ld_preloading/syscall_intercept/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building ASM object test/CMakeFiles/pattern3.out.dir/pattern3.out.S.o"
	cd /home/titan/Titan/ld_preloading/syscall_intercept/build/test && /usr/bin/gcc $(ASM_DEFINES) $(ASM_INCLUDES) $(ASM_FLAGS) -o CMakeFiles/pattern3.out.dir/pattern3.out.S.o -c /home/titan/Titan/ld_preloading/syscall_intercept/test/pattern3.out.S

# Object files for target pattern3.out
pattern3_out_OBJECTS = \
"CMakeFiles/pattern3.out.dir/pattern3.out.S.o"

# External object files for target pattern3.out
pattern3_out_EXTERNAL_OBJECTS =

test/libpattern3.out.so: test/CMakeFiles/pattern3.out.dir/pattern3.out.S.o
test/libpattern3.out.so: test/CMakeFiles/pattern3.out.dir/build.make
test/libpattern3.out.so: test/CMakeFiles/pattern3.out.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/titan/Titan/ld_preloading/syscall_intercept/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking ASM shared library libpattern3.out.so"
	cd /home/titan/Titan/ld_preloading/syscall_intercept/build/test && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/pattern3.out.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
test/CMakeFiles/pattern3.out.dir/build: test/libpattern3.out.so

.PHONY : test/CMakeFiles/pattern3.out.dir/build

test/CMakeFiles/pattern3.out.dir/clean:
	cd /home/titan/Titan/ld_preloading/syscall_intercept/build/test && $(CMAKE_COMMAND) -P CMakeFiles/pattern3.out.dir/cmake_clean.cmake
.PHONY : test/CMakeFiles/pattern3.out.dir/clean

test/CMakeFiles/pattern3.out.dir/depend:
	cd /home/titan/Titan/ld_preloading/syscall_intercept/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/titan/Titan/ld_preloading/syscall_intercept /home/titan/Titan/ld_preloading/syscall_intercept/test /home/titan/Titan/ld_preloading/syscall_intercept/build /home/titan/Titan/ld_preloading/syscall_intercept/build/test /home/titan/Titan/ld_preloading/syscall_intercept/build/test/CMakeFiles/pattern3.out.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : test/CMakeFiles/pattern3.out.dir/depend

