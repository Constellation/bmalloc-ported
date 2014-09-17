cmake_minimum_required(VERSION 2.8)

project(bmalloc C CXX)

if(COMMAND cmake_policy)
    cmake_policy(SET CMP0003 NEW)  # don't split absolute link paths
    cmake_policy(SET CMP0012 NEW)  # recognize number & boolean literals
    cmake_policy(SET CMP0015 NEW)  # convert relative link paths to absolute
endif(COMMAND cmake_policy)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
find_package(Threads)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    # using Clang
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_definitions("-fpermissive")
endif()

file(GLOB src "bmalloc/*.cpp")
add_library(bmalloc STATIC ${src})

include_directories(bmalloc)
file(GLOB test_src "test/*.cpp")
add_executable(test ${test_src})
target_link_libraries(test ${CMAKE_THREAD_LIBS_INIT} bmalloc)
