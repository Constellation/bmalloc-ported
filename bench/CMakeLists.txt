# benchmarks

add_subdirectory("google-benchmark")

include_directories("${PROJECT_SOURCE_DIR}/bmalloc")
link_directories("google-benchmark")
include_directories("google-benchmark/include")

file(GLOB src "*.cpp")
add_executable(benchmarks ${src})
target_link_libraries(benchmarks google-benchmark ${BMALLOC_LIBS})
