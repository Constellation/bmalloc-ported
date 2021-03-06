cmake_minimum_required(VERSION 2.8)

project(bmalloc C CXX)

include(ExternalProject)

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
if (CMAKE_BUILD_TYPE MATCHES Release)
    add_definitions("-DNDEBUG")
endif()

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

set(bmalloc_SOURCES
    bmalloc/Allocator.cpp
    bmalloc/Cache.cpp
    bmalloc/Deallocator.cpp
    bmalloc/Environment.cpp
    bmalloc/FreeList.cpp
    bmalloc/Heap.cpp
    bmalloc/ObjectType.cpp
    bmalloc/SegregatedFreeList.cpp
    bmalloc/StaticMutex.cpp
    bmalloc/VMHeap.cpp
    bmalloc/mbmalloc.cpp
)

if (CMAKE_HOST_APPLE)
    list(APPEND bmalloc_SOURCES
        bmalloc/Zone.cpp
    )
endif()

add_library(bmalloc STATIC ${bmalloc_SOURCES})

set(BMALLOC_LIBS ${CMAKE_THREAD_LIBS_INIT} bmalloc)
include_directories(".")

include_directories(${CMAKE_CURRENT_BINARY_DIR}/include)
link_directories(${CMAKE_CURRENT_BINARY_DIR}/lib)

ExternalProject_Add(project-jemalloc
    URL http://www.canonware.com/download/jemalloc/jemalloc-3.6.0.tar.bz2
    CONFIGURE_COMMAND <SOURCE_DIR>/configure CC=${CMAKE_C_COMPILER} ${CMAKE_C_COMPILER_ARG1} --prefix=<INSTALL_DIR> --with-private-namespace=je_internal_ --with-jemalloc-prefix=je_ --enable-cc-silence
    INSTALL_DIR ${PROJECT_BINARY_DIR}
    BUILD_BYPRODUCTS ${PROJECT_BINARY_DIR}/lib/libjemalloc_pic.a
)
add_library(jemalloc STATIC IMPORTED GLOBAL)
set_target_properties(jemalloc PROPERTIES IMPORTED_LOCATION ${PROJECT_BINARY_DIR}/lib/libjemalloc_pic.a)
add_dependencies(jemalloc project-jemalloc)

add_subdirectory(test)
add_subdirectory(bench)
