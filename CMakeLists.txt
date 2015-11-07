cmake_minimum_required (VERSION 2.6)
project (simplegc)

set (DIR_INC "${PROJECT_SOURCE_DIR}/include")
set (DIR_SRC "${PROJECT_SOURCE_DIR}/src")

# Compiler options
set ( CMAKE_C_FLAGS "-lrt -Wall -std=gnu99 -O2 -g")

# add the binary tree to the search path for include files
# so that we will find config.h
include_directories("${DIR_INC}")

 
add_subdirectory ("${DIR_SRC}")
