# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 2.8

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canoncical targets will work.
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
CMAKE_COMMAND = "/Applications/CMake 2.8-5.app/Contents/bin/cmake"

# The command to remove a file.
RM = "/Applications/CMake 2.8-5.app/Contents/bin/cmake" -E remove -f

# The program to use to edit the cache.
CMAKE_EDIT_COMMAND = "/Applications/CMake 2.8-5.app/Contents/bin/ccmake"

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c-6.1.1-src

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c-6.1.1-src

# Include any dependencies generated for this target.
include strings/CMakeFiles/conf_to_src.dir/depend.make

# Include the progress variables for this target.
include strings/CMakeFiles/conf_to_src.dir/progress.make

# Include the compile flags for this target's objects.
include strings/CMakeFiles/conf_to_src.dir/flags.make

strings/CMakeFiles/conf_to_src.dir/conf_to_src.c.o: strings/CMakeFiles/conf_to_src.dir/flags.make
strings/CMakeFiles/conf_to_src.dir/conf_to_src.c.o: strings/conf_to_src.c
	$(CMAKE_COMMAND) -E cmake_progress_report /Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c-6.1.1-src/CMakeFiles $(CMAKE_PROGRESS_1)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building C object strings/CMakeFiles/conf_to_src.dir/conf_to_src.c.o"
	cd /Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c-6.1.1-src/strings && /usr/bin/gcc  $(C_DEFINES) $(C_FLAGS) -o CMakeFiles/conf_to_src.dir/conf_to_src.c.o   -c /Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c-6.1.1-src/strings/conf_to_src.c

strings/CMakeFiles/conf_to_src.dir/conf_to_src.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/conf_to_src.dir/conf_to_src.c.i"
	cd /Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c-6.1.1-src/strings && /usr/bin/gcc  $(C_DEFINES) $(C_FLAGS) -E /Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c-6.1.1-src/strings/conf_to_src.c > CMakeFiles/conf_to_src.dir/conf_to_src.c.i

strings/CMakeFiles/conf_to_src.dir/conf_to_src.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/conf_to_src.dir/conf_to_src.c.s"
	cd /Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c-6.1.1-src/strings && /usr/bin/gcc  $(C_DEFINES) $(C_FLAGS) -S /Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c-6.1.1-src/strings/conf_to_src.c -o CMakeFiles/conf_to_src.dir/conf_to_src.c.s

strings/CMakeFiles/conf_to_src.dir/conf_to_src.c.o.requires:
.PHONY : strings/CMakeFiles/conf_to_src.dir/conf_to_src.c.o.requires

strings/CMakeFiles/conf_to_src.dir/conf_to_src.c.o.provides: strings/CMakeFiles/conf_to_src.dir/conf_to_src.c.o.requires
	$(MAKE) -f strings/CMakeFiles/conf_to_src.dir/build.make strings/CMakeFiles/conf_to_src.dir/conf_to_src.c.o.provides.build
.PHONY : strings/CMakeFiles/conf_to_src.dir/conf_to_src.c.o.provides

strings/CMakeFiles/conf_to_src.dir/conf_to_src.c.o.provides.build: strings/CMakeFiles/conf_to_src.dir/conf_to_src.c.o

# Object files for target conf_to_src
conf_to_src_OBJECTS = \
"CMakeFiles/conf_to_src.dir/conf_to_src.c.o"

# External object files for target conf_to_src
conf_to_src_EXTERNAL_OBJECTS =

strings/conf_to_src: strings/CMakeFiles/conf_to_src.dir/conf_to_src.c.o
strings/conf_to_src: strings/libstrings.a
strings/conf_to_src: strings/CMakeFiles/conf_to_src.dir/build.make
strings/conf_to_src: strings/CMakeFiles/conf_to_src.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --red --bold "Linking C executable conf_to_src"
	cd /Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c-6.1.1-src/strings && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/conf_to_src.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
strings/CMakeFiles/conf_to_src.dir/build: strings/conf_to_src
.PHONY : strings/CMakeFiles/conf_to_src.dir/build

strings/CMakeFiles/conf_to_src.dir/requires: strings/CMakeFiles/conf_to_src.dir/conf_to_src.c.o.requires
.PHONY : strings/CMakeFiles/conf_to_src.dir/requires

strings/CMakeFiles/conf_to_src.dir/clean:
	cd /Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c-6.1.1-src/strings && $(CMAKE_COMMAND) -P CMakeFiles/conf_to_src.dir/cmake_clean.cmake
.PHONY : strings/CMakeFiles/conf_to_src.dir/clean

strings/CMakeFiles/conf_to_src.dir/depend:
	cd /Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c-6.1.1-src && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c-6.1.1-src /Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c-6.1.1-src/strings /Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c-6.1.1-src /Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c-6.1.1-src/strings /Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c-6.1.1-src/strings/CMakeFiles/conf_to_src.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : strings/CMakeFiles/conf_to_src.dir/depend

