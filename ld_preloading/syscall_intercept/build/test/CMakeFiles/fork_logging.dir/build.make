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
include test/CMakeFiles/fork_logging.dir/depend.make

# Include the progress variables for this target.
include test/CMakeFiles/fork_logging.dir/progress.make

# Include the compile flags for this target's objects.
include test/CMakeFiles/fork_logging.dir/flags.make

test/CMakeFiles/fork_logging.dir/fork_logging.c.o: test/CMakeFiles/fork_logging.dir/flags.make
test/CMakeFiles/fork_logging.dir/fork_logging.c.o: ../test/fork_logging.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/titan/Titan/ld_preloading/syscall_intercept/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building C object test/CMakeFiles/fork_logging.dir/fork_logging.c.o"
	cd /home/titan/Titan/ld_preloading/syscall_intercept/build/test && /usr/bin/gcc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/fork_logging.dir/fork_logging.c.o   -c /home/titan/Titan/ld_preloading/syscall_intercept/test/fork_logging.c

test/CMakeFiles/fork_logging.dir/fork_logging.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/fork_logging.dir/fork_logging.c.i"
	cd /home/titan/Titan/ld_preloading/syscall_intercept/build/test && /usr/bin/gcc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/titan/Titan/ld_preloading/syscall_intercept/test/fork_logging.c > CMakeFiles/fork_logging.dir/fork_logging.c.i

test/CMakeFiles/fork_logging.dir/fork_logging.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/fork_logging.dir/fork_logging.c.s"
	cd /home/titan/Titan/ld_preloading/syscall_intercept/build/test && /usr/bin/gcc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/titan/Titan/ld_preloading/syscall_intercept/test/fork_logging.c -o CMakeFiles/fork_logging.dir/fork_logging.c.s

test/CMakeFiles/fork_logging.dir/fork_logging.c.o.requires:

.PHONY : test/CMakeFiles/fork_logging.dir/fork_logging.c.o.requires

test/CMakeFiles/fork_logging.dir/fork_logging.c.o.provides: test/CMakeFiles/fork_logging.dir/fork_logging.c.o.requires
	$(MAKE) -f test/CMakeFiles/fork_logging.dir/build.make test/CMakeFiles/fork_logging.dir/fork_logging.c.o.provides.build
.PHONY : test/CMakeFiles/fork_logging.dir/fork_logging.c.o.provides

test/CMakeFiles/fork_logging.dir/fork_logging.c.o.provides.build: test/CMakeFiles/fork_logging.dir/fork_logging.c.o


# Object files for target fork_logging
fork_logging_OBJECTS = \
"CMakeFiles/fork_logging.dir/fork_logging.c.o"

# External object files for target fork_logging
fork_logging_EXTERNAL_OBJECTS =

test/fork_logging: test/CMakeFiles/fork_logging.dir/fork_logging.c.o
test/fork_logging: test/CMakeFiles/fork_logging.dir/build.make
test/fork_logging: test/CMakeFiles/fork_logging.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/titan/Titan/ld_preloading/syscall_intercept/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking C executable fork_logging"
	cd /home/titan/Titan/ld_preloading/syscall_intercept/build/test && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/fork_logging.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
test/CMakeFiles/fork_logging.dir/build: test/fork_logging

.PHONY : test/CMakeFiles/fork_logging.dir/build

test/CMakeFiles/fork_logging.dir/requires: test/CMakeFiles/fork_logging.dir/fork_logging.c.o.requires

.PHONY : test/CMakeFiles/fork_logging.dir/requires

test/CMakeFiles/fork_logging.dir/clean:
	cd /home/titan/Titan/ld_preloading/syscall_intercept/build/test && $(CMAKE_COMMAND) -P CMakeFiles/fork_logging.dir/cmake_clean.cmake
.PHONY : test/CMakeFiles/fork_logging.dir/clean

test/CMakeFiles/fork_logging.dir/depend:
	cd /home/titan/Titan/ld_preloading/syscall_intercept/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/titan/Titan/ld_preloading/syscall_intercept /home/titan/Titan/ld_preloading/syscall_intercept/test /home/titan/Titan/ld_preloading/syscall_intercept/build /home/titan/Titan/ld_preloading/syscall_intercept/build/test /home/titan/Titan/ld_preloading/syscall_intercept/build/test/CMakeFiles/fork_logging.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : test/CMakeFiles/fork_logging.dir/depend

