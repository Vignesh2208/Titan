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
include test/CMakeFiles/nosyscall.in.dir/depend.make

# Include the progress variables for this target.
include test/CMakeFiles/nosyscall.in.dir/progress.make

# Include the compile flags for this target's objects.
include test/CMakeFiles/nosyscall.in.dir/flags.make

test/CMakeFiles/nosyscall.in.dir/nosyscall.in.S.o: test/CMakeFiles/nosyscall.in.dir/flags.make
test/CMakeFiles/nosyscall.in.dir/nosyscall.in.S.o: ../test/nosyscall.in.S
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/vignesh/Desktop/Lookahead_Testing/syscall_intercept/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building ASM object test/CMakeFiles/nosyscall.in.dir/nosyscall.in.S.o"
	cd /home/vignesh/Desktop/Lookahead_Testing/syscall_intercept/build/test && /usr/bin/clang  $(ASM_DEFINES) $(ASM_INCLUDES) $(ASM_FLAGS) -o CMakeFiles/nosyscall.in.dir/nosyscall.in.S.o -c /home/vignesh/Desktop/Lookahead_Testing/syscall_intercept/test/nosyscall.in.S

test/CMakeFiles/nosyscall.in.dir/nosyscall.in.S.o.requires:

.PHONY : test/CMakeFiles/nosyscall.in.dir/nosyscall.in.S.o.requires

test/CMakeFiles/nosyscall.in.dir/nosyscall.in.S.o.provides: test/CMakeFiles/nosyscall.in.dir/nosyscall.in.S.o.requires
	$(MAKE) -f test/CMakeFiles/nosyscall.in.dir/build.make test/CMakeFiles/nosyscall.in.dir/nosyscall.in.S.o.provides.build
.PHONY : test/CMakeFiles/nosyscall.in.dir/nosyscall.in.S.o.provides

test/CMakeFiles/nosyscall.in.dir/nosyscall.in.S.o.provides.build: test/CMakeFiles/nosyscall.in.dir/nosyscall.in.S.o


# Object files for target nosyscall.in
nosyscall_in_OBJECTS = \
"CMakeFiles/nosyscall.in.dir/nosyscall.in.S.o"

# External object files for target nosyscall.in
nosyscall_in_EXTERNAL_OBJECTS =

test/libnosyscall.in.so: test/CMakeFiles/nosyscall.in.dir/nosyscall.in.S.o
test/libnosyscall.in.so: test/CMakeFiles/nosyscall.in.dir/build.make
test/libnosyscall.in.so: test/CMakeFiles/nosyscall.in.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/vignesh/Desktop/Lookahead_Testing/syscall_intercept/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking ASM shared library libnosyscall.in.so"
	cd /home/vignesh/Desktop/Lookahead_Testing/syscall_intercept/build/test && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/nosyscall.in.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
test/CMakeFiles/nosyscall.in.dir/build: test/libnosyscall.in.so

.PHONY : test/CMakeFiles/nosyscall.in.dir/build

test/CMakeFiles/nosyscall.in.dir/requires: test/CMakeFiles/nosyscall.in.dir/nosyscall.in.S.o.requires

.PHONY : test/CMakeFiles/nosyscall.in.dir/requires

test/CMakeFiles/nosyscall.in.dir/clean:
	cd /home/vignesh/Desktop/Lookahead_Testing/syscall_intercept/build/test && $(CMAKE_COMMAND) -P CMakeFiles/nosyscall.in.dir/cmake_clean.cmake
.PHONY : test/CMakeFiles/nosyscall.in.dir/clean

test/CMakeFiles/nosyscall.in.dir/depend:
	cd /home/vignesh/Desktop/Lookahead_Testing/syscall_intercept/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/vignesh/Desktop/Lookahead_Testing/syscall_intercept /home/vignesh/Desktop/Lookahead_Testing/syscall_intercept/test /home/vignesh/Desktop/Lookahead_Testing/syscall_intercept/build /home/vignesh/Desktop/Lookahead_Testing/syscall_intercept/build/test /home/vignesh/Desktop/Lookahead_Testing/syscall_intercept/build/test/CMakeFiles/nosyscall.in.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : test/CMakeFiles/nosyscall.in.dir/depend

