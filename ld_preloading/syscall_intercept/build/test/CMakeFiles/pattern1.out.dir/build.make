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
include test/CMakeFiles/pattern1.out.dir/depend.make

# Include the progress variables for this target.
include test/CMakeFiles/pattern1.out.dir/progress.make

# Include the compile flags for this target's objects.
include test/CMakeFiles/pattern1.out.dir/flags.make

test/CMakeFiles/pattern1.out.dir/pattern1.out.S.o: test/CMakeFiles/pattern1.out.dir/flags.make
test/CMakeFiles/pattern1.out.dir/pattern1.out.S.o: ../test/pattern1.out.S
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/vignesh/Desktop/Lookahead_Testing/syscall_intercept/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building ASM object test/CMakeFiles/pattern1.out.dir/pattern1.out.S.o"
	cd /home/vignesh/Desktop/Lookahead_Testing/syscall_intercept/build/test && /usr/bin/clang  $(ASM_DEFINES) $(ASM_INCLUDES) $(ASM_FLAGS) -o CMakeFiles/pattern1.out.dir/pattern1.out.S.o -c /home/vignesh/Desktop/Lookahead_Testing/syscall_intercept/test/pattern1.out.S

test/CMakeFiles/pattern1.out.dir/pattern1.out.S.o.requires:

.PHONY : test/CMakeFiles/pattern1.out.dir/pattern1.out.S.o.requires

test/CMakeFiles/pattern1.out.dir/pattern1.out.S.o.provides: test/CMakeFiles/pattern1.out.dir/pattern1.out.S.o.requires
	$(MAKE) -f test/CMakeFiles/pattern1.out.dir/build.make test/CMakeFiles/pattern1.out.dir/pattern1.out.S.o.provides.build
.PHONY : test/CMakeFiles/pattern1.out.dir/pattern1.out.S.o.provides

test/CMakeFiles/pattern1.out.dir/pattern1.out.S.o.provides.build: test/CMakeFiles/pattern1.out.dir/pattern1.out.S.o


# Object files for target pattern1.out
pattern1_out_OBJECTS = \
"CMakeFiles/pattern1.out.dir/pattern1.out.S.o"

# External object files for target pattern1.out
pattern1_out_EXTERNAL_OBJECTS =

test/libpattern1.out.so: test/CMakeFiles/pattern1.out.dir/pattern1.out.S.o
test/libpattern1.out.so: test/CMakeFiles/pattern1.out.dir/build.make
test/libpattern1.out.so: test/CMakeFiles/pattern1.out.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/vignesh/Desktop/Lookahead_Testing/syscall_intercept/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking ASM shared library libpattern1.out.so"
	cd /home/vignesh/Desktop/Lookahead_Testing/syscall_intercept/build/test && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/pattern1.out.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
test/CMakeFiles/pattern1.out.dir/build: test/libpattern1.out.so

.PHONY : test/CMakeFiles/pattern1.out.dir/build

test/CMakeFiles/pattern1.out.dir/requires: test/CMakeFiles/pattern1.out.dir/pattern1.out.S.o.requires

.PHONY : test/CMakeFiles/pattern1.out.dir/requires

test/CMakeFiles/pattern1.out.dir/clean:
	cd /home/vignesh/Desktop/Lookahead_Testing/syscall_intercept/build/test && $(CMAKE_COMMAND) -P CMakeFiles/pattern1.out.dir/cmake_clean.cmake
.PHONY : test/CMakeFiles/pattern1.out.dir/clean

test/CMakeFiles/pattern1.out.dir/depend:
	cd /home/vignesh/Desktop/Lookahead_Testing/syscall_intercept/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/vignesh/Desktop/Lookahead_Testing/syscall_intercept /home/vignesh/Desktop/Lookahead_Testing/syscall_intercept/test /home/vignesh/Desktop/Lookahead_Testing/syscall_intercept/build /home/vignesh/Desktop/Lookahead_Testing/syscall_intercept/build/test /home/vignesh/Desktop/Lookahead_Testing/syscall_intercept/build/test/CMakeFiles/pattern1.out.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : test/CMakeFiles/pattern1.out.dir/depend

