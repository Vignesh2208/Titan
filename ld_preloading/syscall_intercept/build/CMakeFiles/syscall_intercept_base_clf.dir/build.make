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
include CMakeFiles/syscall_intercept_base_clf.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/syscall_intercept_base_clf.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/syscall_intercept_base_clf.dir/flags.make

CMakeFiles/syscall_intercept_base_clf.dir/src/cmdline_filter.c.o: CMakeFiles/syscall_intercept_base_clf.dir/flags.make
CMakeFiles/syscall_intercept_base_clf.dir/src/cmdline_filter.c.o: ../src/cmdline_filter.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/titan/Titan/ld_preloading/syscall_intercept/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building C object CMakeFiles/syscall_intercept_base_clf.dir/src/cmdline_filter.c.o"
	/usr/bin/gcc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/syscall_intercept_base_clf.dir/src/cmdline_filter.c.o   -c /home/titan/Titan/ld_preloading/syscall_intercept/src/cmdline_filter.c

CMakeFiles/syscall_intercept_base_clf.dir/src/cmdline_filter.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/syscall_intercept_base_clf.dir/src/cmdline_filter.c.i"
	/usr/bin/gcc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/titan/Titan/ld_preloading/syscall_intercept/src/cmdline_filter.c > CMakeFiles/syscall_intercept_base_clf.dir/src/cmdline_filter.c.i

CMakeFiles/syscall_intercept_base_clf.dir/src/cmdline_filter.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/syscall_intercept_base_clf.dir/src/cmdline_filter.c.s"
	/usr/bin/gcc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/titan/Titan/ld_preloading/syscall_intercept/src/cmdline_filter.c -o CMakeFiles/syscall_intercept_base_clf.dir/src/cmdline_filter.c.s

syscall_intercept_base_clf: CMakeFiles/syscall_intercept_base_clf.dir/src/cmdline_filter.c.o
syscall_intercept_base_clf: CMakeFiles/syscall_intercept_base_clf.dir/build.make

.PHONY : syscall_intercept_base_clf

# Rule to build all files generated by this target.
CMakeFiles/syscall_intercept_base_clf.dir/build: syscall_intercept_base_clf

.PHONY : CMakeFiles/syscall_intercept_base_clf.dir/build

CMakeFiles/syscall_intercept_base_clf.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/syscall_intercept_base_clf.dir/cmake_clean.cmake
.PHONY : CMakeFiles/syscall_intercept_base_clf.dir/clean

CMakeFiles/syscall_intercept_base_clf.dir/depend:
	cd /home/titan/Titan/ld_preloading/syscall_intercept/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/titan/Titan/ld_preloading/syscall_intercept /home/titan/Titan/ld_preloading/syscall_intercept /home/titan/Titan/ld_preloading/syscall_intercept/build /home/titan/Titan/ld_preloading/syscall_intercept/build /home/titan/Titan/ld_preloading/syscall_intercept/build/CMakeFiles/syscall_intercept_base_clf.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/syscall_intercept_base_clf.dir/depend

