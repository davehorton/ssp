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
CMAKE_SOURCE_DIR = /Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c++-1.1.3

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c++-1.1.3

# Include any dependencies generated for this target.
include examples/CMakeFiles/dynamic_load.dir/depend.make

# Include the progress variables for this target.
include examples/CMakeFiles/dynamic_load.dir/progress.make

# Include the compile flags for this target's objects.
include examples/CMakeFiles/dynamic_load.dir/flags.make

examples/CMakeFiles/dynamic_load.dir/dynamic_load.cpp.o: examples/CMakeFiles/dynamic_load.dir/flags.make
examples/CMakeFiles/dynamic_load.dir/dynamic_load.cpp.o: examples/dynamic_load.cpp
	$(CMAKE_COMMAND) -E cmake_progress_report /Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c++-1.1.3/CMakeFiles $(CMAKE_PROGRESS_1)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object examples/CMakeFiles/dynamic_load.dir/dynamic_load.cpp.o"
	cd /Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c++-1.1.3/examples && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS)    -I/usr/local/mysql/include -o CMakeFiles/dynamic_load.dir/dynamic_load.cpp.o -c /Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c++-1.1.3/examples/dynamic_load.cpp

examples/CMakeFiles/dynamic_load.dir/dynamic_load.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/dynamic_load.dir/dynamic_load.cpp.i"
	cd /Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c++-1.1.3/examples && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS)    -I/usr/local/mysql/include -E /Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c++-1.1.3/examples/dynamic_load.cpp > CMakeFiles/dynamic_load.dir/dynamic_load.cpp.i

examples/CMakeFiles/dynamic_load.dir/dynamic_load.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/dynamic_load.dir/dynamic_load.cpp.s"
	cd /Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c++-1.1.3/examples && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS)    -I/usr/local/mysql/include -S /Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c++-1.1.3/examples/dynamic_load.cpp -o CMakeFiles/dynamic_load.dir/dynamic_load.cpp.s

examples/CMakeFiles/dynamic_load.dir/dynamic_load.cpp.o.requires:
.PHONY : examples/CMakeFiles/dynamic_load.dir/dynamic_load.cpp.o.requires

examples/CMakeFiles/dynamic_load.dir/dynamic_load.cpp.o.provides: examples/CMakeFiles/dynamic_load.dir/dynamic_load.cpp.o.requires
	$(MAKE) -f examples/CMakeFiles/dynamic_load.dir/build.make examples/CMakeFiles/dynamic_load.dir/dynamic_load.cpp.o.provides.build
.PHONY : examples/CMakeFiles/dynamic_load.dir/dynamic_load.cpp.o.provides

examples/CMakeFiles/dynamic_load.dir/dynamic_load.cpp.o.provides.build: examples/CMakeFiles/dynamic_load.dir/dynamic_load.cpp.o

# Object files for target dynamic_load
dynamic_load_OBJECTS = \
"CMakeFiles/dynamic_load.dir/dynamic_load.cpp.o"

# External object files for target dynamic_load
dynamic_load_EXTERNAL_OBJECTS =

examples/dynamic_load: examples/CMakeFiles/dynamic_load.dir/dynamic_load.cpp.o
examples/dynamic_load: driver/libmysqlcppconn-static.a
examples/dynamic_load: examples/CMakeFiles/dynamic_load.dir/build.make
examples/dynamic_load: examples/CMakeFiles/dynamic_load.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --red --bold "Linking CXX executable dynamic_load"
	cd /Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c++-1.1.3/examples && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/dynamic_load.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
examples/CMakeFiles/dynamic_load.dir/build: examples/dynamic_load
.PHONY : examples/CMakeFiles/dynamic_load.dir/build

examples/CMakeFiles/dynamic_load.dir/requires: examples/CMakeFiles/dynamic_load.dir/dynamic_load.cpp.o.requires
.PHONY : examples/CMakeFiles/dynamic_load.dir/requires

examples/CMakeFiles/dynamic_load.dir/clean:
	cd /Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c++-1.1.3/examples && $(CMAKE_COMMAND) -P CMakeFiles/dynamic_load.dir/cmake_clean.cmake
.PHONY : examples/CMakeFiles/dynamic_load.dir/clean

examples/CMakeFiles/dynamic_load.dir/depend:
	cd /Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c++-1.1.3 && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c++-1.1.3 /Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c++-1.1.3/examples /Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c++-1.1.3 /Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c++-1.1.3/examples /Users/dhorton/beachdog-enterprises/beachdog-networks/git/ssp/deps/mysql-connector-c++-1.1.3/examples/CMakeFiles/dynamic_load.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : examples/CMakeFiles/dynamic_load.dir/depend

