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
include test/CMakeFiles/hook_test_preload_with_static.dir/depend.make

# Include the progress variables for this target.
include test/CMakeFiles/hook_test_preload_with_static.dir/progress.make

# Include the compile flags for this target's objects.
include test/CMakeFiles/hook_test_preload_with_static.dir/flags.make

# Object files for target hook_test_preload_with_static
hook_test_preload_with_static_OBJECTS =

# External object files for target hook_test_preload_with_static
hook_test_preload_with_static_EXTERNAL_OBJECTS = \
"/home/titan/Titan/ld_preloading/syscall_intercept/build/test/CMakeFiles/hook_test_preload_o.dir/hook_test_preload.c.o"

test/libhook_test_preload_with_static.so: test/CMakeFiles/hook_test_preload_o.dir/hook_test_preload.c.o
test/libhook_test_preload_with_static.so: test/CMakeFiles/hook_test_preload_with_static.dir/build.make
test/libhook_test_preload_with_static.so: libsyscall_intercept.a
test/libhook_test_preload_with_static.so: test/CMakeFiles/hook_test_preload_with_static.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/titan/Titan/ld_preloading/syscall_intercept/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Linking C shared library libhook_test_preload_with_static.so"
	cd /home/titan/Titan/ld_preloading/syscall_intercept/build/test && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/hook_test_preload_with_static.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
test/CMakeFiles/hook_test_preload_with_static.dir/build: test/libhook_test_preload_with_static.so

.PHONY : test/CMakeFiles/hook_test_preload_with_static.dir/build

test/CMakeFiles/hook_test_preload_with_static.dir/requires:

.PHONY : test/CMakeFiles/hook_test_preload_with_static.dir/requires

test/CMakeFiles/hook_test_preload_with_static.dir/clean:
	cd /home/titan/Titan/ld_preloading/syscall_intercept/build/test && $(CMAKE_COMMAND) -P CMakeFiles/hook_test_preload_with_static.dir/cmake_clean.cmake
.PHONY : test/CMakeFiles/hook_test_preload_with_static.dir/clean

test/CMakeFiles/hook_test_preload_with_static.dir/depend:
	cd /home/titan/Titan/ld_preloading/syscall_intercept/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/titan/Titan/ld_preloading/syscall_intercept /home/titan/Titan/ld_preloading/syscall_intercept/test /home/titan/Titan/ld_preloading/syscall_intercept/build /home/titan/Titan/ld_preloading/syscall_intercept/build/test /home/titan/Titan/ld_preloading/syscall_intercept/build/test/CMakeFiles/hook_test_preload_with_static.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : test/CMakeFiles/hook_test_preload_with_static.dir/depend

