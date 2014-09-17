cmake_minimum_required(VERSION 2.8)

project(bmalloc C CXX)

if(COMMAND cmake_policy)
    cmake_policy(SET CMP0003 NEW)  # don't split absolute link paths
    cmake_policy(SET CMP0012 NEW)  # recognize number & boolean literals
    cmake_policy(SET CMP0015 NEW)  # convert relative link paths to absolute
endif(COMMAND cmake_policy)

# We can specify Release / Debug / None
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

add_definitions(
    "-Wall"
    "-Wextra"
    "-Wno-unused-parameter"
    "-Wwrite-strings"
    "-Wreturn-type"
    "-Wpointer-arith"
    "-Wno-unused-variable"
    "-Wwrite-strings"
    "-Wno-long-long"
    "-Wno-missing-field-initializers"
    "-g3"
)

find_package(Threads)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    # using Clang
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_definitions("-fpermissive")
endif()

file(GLOB src "bmalloc/*.cpp")
add_library(bmalloc STATIC ${src})

set(BMALLOC_LIBS ${CMAKE_THREAD_LIBS_INIT} bmalloc)
include_directories("utility")

add_subdirectory(test)
add_subdirectory(bench)
