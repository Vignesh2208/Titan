# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.5

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
CMAKE_SOURCE_DIR = /home/vignesh/Desktop/Lookahead_Testing/syscall_intercept

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/vignesh/Desktop/Lookahead_Testing/syscall_intercept/build

# Include any dependencies generated for this target.
include examples/CMakeFiles/syscall_logger.dir/depend.make

# Include the progress variables for this target.
include examples/CMakeFiles/syscall_logger.dir/progress.make

# Include the compile flags for this target's objects.
include examples/CMakeFiles/syscall_logger.dir/flags.make

examples/CMakeFiles/syscall_logger.dir/syscall_logger.c.o: examples/CMakeFiles/syscall_logger.dir/flags.make
examples/CMakeFiles/syscall_logger.dir/syscall_logger.c.o: ../examples/syscall_logger.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/vignesh/Desktop/Lookahead_Testing/syscall_intercept/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building C object examples/CMakeFiles/syscall_logger.dir/syscall_logger.c.o"
	cd /home/vignesh/Desktop/Lookahead_Testing/syscall_intercept/build/examples && /usr/bin/clang  $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/syscall_logger.dir/syscall_logger.c.o   -c /home/vignesh/Desktop/Lookahead_Testing/syscall_intercept/examples/syscall_logger.c

examples/CMakeFiles/syscall_logger.dir/syscall_logger.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/syscall_logger.dir/syscall_logger.c.i"
	cd /home/vignesh/Desktop/Lookahead_Testing/syscall_intercept/build/examples && /usr/bin/clang  $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/vignesh/Desktop/Lookahead_Testing/syscall_intercept/examples/syscall_logger.c > CMakeFiles/syscall_logger.dir/syscall_logger.c.i

examples/CMakeFiles/syscall_logger.dir/syscall_logger.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/syscall_logger.dir/syscall_logger.c.s"
	cd /home/vignesh/Desktop/Lookahead_Testing/syscall_intercept/build/examples && /usr/bin/clang  $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/vignesh/Desktop/Lookahead_Testing/syscall_intercept/examples/syscall_logger.c -o CMakeFiles/syscall_logger.dir/syscall_logger.c.s

examples/CMakeFiles/syscall_logger.dir/syscall_logger.c.o.requires:

.PHONY : examples/CMakeFiles/syscall_logger.dir/syscall_logger.c.o.requires

examples/CMakeFiles/syscall_logger.dir/syscall_logger.c.o.provides: examples/CMakeFiles/syscall_logger.dir/syscall_logger.c.o.requires
	$(MAKE) -f examples/CMakeFiles/syscall_logger.dir/build.make examples/CMakeFiles/syscall_logger.dir/syscall_logger.c.o.provides.build
.PHONY : examples/CMakeFiles/syscall_logger.dir/syscall_logger.c.o.provides

examples/CMakeFiles/syscall_logger.dir/syscall_logger.c.o.provides.build: examples/CMakeFiles/syscall_logger.dir/syscall_logger.c.o


examples/CMakeFiles/syscall_logger.dir/syscall_desc.c.o: examples/CMakeFiles/syscall_logger.dir/flags.make
examples/CMakeFiles/syscall_logger.dir/syscall_desc.c.o: ../examples/syscall_desc.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/vignesh/Desktop/Lookahead_Testing/syscall_intercept/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building C object examples/CMakeFiles/syscall_logger.dir/syscall_desc.c.o"
	cd /home/vignesh/Desktop/Lookahead_Testing/syscall_intercept/build/examples && /usr/bin/clang  $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/syscall_logger.dir/syscall_desc.c.o   -c /home/vignesh/Desktop/Lookahead_Testing/syscall_intercept/examples/syscall_desc.c

examples/CMakeFiles/syscall_logger.dir/syscall_desc.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/syscall_logger.dir/syscall_desc.c.i"
	cd /home/vignesh/Desktop/Lookahead_Testing/syscall_intercept/build/examples && /usr/bin/clang  $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/vignesh/Desktop/Lookahead_Testing/syscall_intercept/examples/syscall_desc.c > CMakeFiles/syscall_logger.dir/syscall_desc.c.i

examples/CMakeFiles/syscall_logger.dir/syscall_desc.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/syscall_logger.dir/syscall_desc.c.s"
	cd /home/vignesh/Desktop/Lookahead_Testing/syscall_intercept/build/examples && /usr/bin/clang  $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/vignesh/Desktop/Lookahead_Testing/syscall_intercept/examples/syscall_desc.c -o CMakeFiles/syscall_logger.dir/syscall_desc.c.s

examples/CMakeFiles/syscall_logger.dir/syscall_desc.c.o.requires:

.PHONY : examples/CMakeFiles/syscall_logger.dir/syscall_desc.c.o.requires

examples/CMakeFiles/syscall_logger.dir/syscall_desc.c.o.provides: examples/CMakeFiles/syscall_logger.dir/syscall_desc.c.o.requires
	$(MAKE) -f examples/CMakeFiles/syscall_logger.dir/build.make examples/CMakeFiles/syscall_logger.dir/syscall_desc.c.o.provides.build
.PHONY : examples/CMakeFiles/syscall_logger.dir/syscall_desc.c.o.provides

examples/CMakeFiles/syscall_logger.dir/syscall_desc.c.o.provides.build: examples/CMakeFiles/syscall_logger.dir/syscall_desc.c.o


# Object files for target syscall_logger
syscall_logger_OBJECTS = \
"CMakeFiles/syscall_logger.dir/syscall_logger.c.o" \
"CMakeFiles/syscall_logger.dir/syscall_desc.c.o"

# External object files for target syscall_logger
syscall_logger_EXTERNAL_OBJECTS =

examples/libsyscall_logger.so: examples/CMakeFiles/syscall_logger.dir/syscall_logger.c.o
examples/libsyscall_logger.so: examples/CMakeFiles/syscall_logger.dir/syscall_desc.c.o
examples/libsyscall_logger.so: examples/CMakeFiles/syscall_logger.dir/build.make
examples/libsyscall_logger.so: libsyscall_intercept.so.0.1.0
examples/libsyscall_logger.so: examples/CMakeFiles/syscall_logger.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/vignesh/Desktop/Lookahead_Testing/syscall_intercept/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Linking C shared library libsyscall_logger.so"
	cd /home/vignesh/Desktop/Lookahead_Testing/syscall_intercept/build/examples && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/syscall_logger.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
examples/CMakeFiles/syscall_logger.dir/build: examples/libsyscall_logger.so

.PHONY : examples/CMakeFiles/syscall_logger.dir/build

examples/CMakeFiles/syscall_logger.dir/requires: examples/CMakeFiles/syscall_logger.dir/syscall_logger.c.o.requires
examples/CMakeFiles/syscall_logger.dir/requires: examples/CMakeFiles/syscall_logger.dir/syscall_desc.c.o.requires

.PHONY : examples/CMakeFiles/syscall_logger.dir/requires

examples/CMakeFiles/syscall_logger.dir/clean:
	cd /home/vignesh/Desktop/Lookahead_Testing/syscall_intercept/build/examples && $(CMAKE_COMMAND) -P CMakeFiles/syscall_logger.dir/cmake_clean.cmake
.PHONY : examples/CMakeFiles/syscall_logger.dir/clean

examples/CMakeFiles/syscall_logger.dir/depend:
	cd /home/vignesh/Desktop/Lookahead_Testing/syscall_intercept/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/vignesh/Desktop/Lookahead_Testing/syscall_intercept /home/vignesh/Desktop/Lookahead_Testing/syscall_intercept/examples /home/vignesh/Desktop/Lookahead_Testing/syscall_intercept/build /home/vignesh/Desktop/Lookahead_Testing/syscall_intercept/build/examples /home/vignesh/Desktop/Lookahead_Testing/syscall_intercept/build/examples/CMakeFiles/syscall_logger.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : examples/CMakeFiles/syscall_logger.dir/depend

