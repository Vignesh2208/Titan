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
CMAKE_SOURCE_DIR = /home/vignesh/Titan/ld_preloading/syscall_intercept

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/vignesh/Titan/ld_preloading/syscall_intercept/build

# Include any dependencies generated for this target.
include CMakeFiles/syscall_intercept_static.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/syscall_intercept_static.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/syscall_intercept_static.dir/flags.make

syscall_intercept_scoped.o: syscall_intercept_unscoped.o
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/home/vignesh/Titan/ld_preloading/syscall_intercept/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Generating syscall_intercept_scoped.o"
	/usr/bin/objcopy --localize-hidden syscall_intercept_unscoped.o syscall_intercept_scoped.o

syscall_intercept_unscoped.o: libsyscall_intercept_unscoped.a
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/home/vignesh/Titan/ld_preloading/syscall_intercept/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Generating syscall_intercept_unscoped.o"
	/usr/bin/ld -r --whole-archive /home/vignesh/Titan/ld_preloading/syscall_intercept/build/libsyscall_intercept_unscoped.a -o syscall_intercept_unscoped.o

# Object files for target syscall_intercept_static
syscall_intercept_static_OBJECTS =

# External object files for target syscall_intercept_static
syscall_intercept_static_EXTERNAL_OBJECTS = \
"/home/vignesh/Titan/ld_preloading/syscall_intercept/build/syscall_intercept_scoped.o"

libsyscall_intercept.a: syscall_intercept_scoped.o
libsyscall_intercept.a: CMakeFiles/syscall_intercept_static.dir/build.make
libsyscall_intercept.a: CMakeFiles/syscall_intercept_static.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/vignesh/Titan/ld_preloading/syscall_intercept/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Linking C static library libsyscall_intercept.a"
	$(CMAKE_COMMAND) -P CMakeFiles/syscall_intercept_static.dir/cmake_clean_target.cmake
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/syscall_intercept_static.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/syscall_intercept_static.dir/build: libsyscall_intercept.a

.PHONY : CMakeFiles/syscall_intercept_static.dir/build

CMakeFiles/syscall_intercept_static.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/syscall_intercept_static.dir/cmake_clean.cmake
.PHONY : CMakeFiles/syscall_intercept_static.dir/clean

CMakeFiles/syscall_intercept_static.dir/depend: syscall_intercept_scoped.o
CMakeFiles/syscall_intercept_static.dir/depend: syscall_intercept_unscoped.o
	cd /home/vignesh/Titan/ld_preloading/syscall_intercept/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/vignesh/Titan/ld_preloading/syscall_intercept /home/vignesh/Titan/ld_preloading/syscall_intercept /home/vignesh/Titan/ld_preloading/syscall_intercept/build /home/vignesh/Titan/ld_preloading/syscall_intercept/build /home/vignesh/Titan/ld_preloading/syscall_intercept/build/CMakeFiles/syscall_intercept_static.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/syscall_intercept_static.dir/depend

