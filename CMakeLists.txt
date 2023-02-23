# SPDX-FileCopyrightText: 2008-2021 HPDCS Group <rootsim@googlegroups.com>
# SPDX-License-Identifier: GPL-3.0-only
cmake_minimum_required(VERSION 3.10)
project("USE" LANGUAGES C DESCRIPTION "Ultimate Share-Everything Parallel Discrete Event Simulator")



set(PROJECT_VERSION 2.0.0)

#set(CMAKE_C_STANDARD 11)
#set(CMAKE_C_STANDARD_REQUIRED ON)
#set(CMAKE_C_VISIBILITY_PRESET hidden)

include(ProcessorCount)
ProcessorCount(N)
if(N EQUAL 0)
  set(N 2)
endif()


exec_program(
getconf ARGS LEVEL1_DCACHE_LINESIZE
OUTPUT_VARIABLE CACHE_LINE_SIZE
)


find_package(Threads REQUIRED)
find_package (Python COMPONENTS Interpreter)

CHECK_INCLUDE_FILE(numa.h HAS_NUMA_H)

add_compile_options(-Wall -Wextra) # -pedantic)

enable_testing()

IF(CMAKE_BUILD_TYPE MATCHES Debug)
  set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/ci/coverage)
  include(CodeCoverage)
  APPEND_COVERAGE_COMPILER_FLAGS()
ENDIF()


#hardware-related macros
add_compile_options(
-DCACHE_LINE_SIZE=${CACHE_LINE_SIZE} 
-DN_CPU=${N} 
-DARCH_X86_64
)



add_subdirectory(src)
add_subdirectory(test)
#add_subdirectory(docs)


IF(CMAKE_BUILD_TYPE MATCHES Debug)
SETUP_TARGET_FOR_COVERAGE_LCOV(
        NAME coverage                 # New target name
        EXECUTABLE ctest -C ${ROOT_DIR}/CTestTestfile.cmake # Executable in PROJECT_BINARY_DIR
)
ENDIF()
