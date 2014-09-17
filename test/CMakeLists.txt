# tests

add_subdirectory("google-test")

include_directories("${PROJECT_SOURCE_DIR}/bmalloc")
link_directories("google-test")
include_directories("google-test")

add_definitions(
    "-Wno-variadic-macros"
    "-fno-strict-aliasing"
)

file(GLOB src "*.cpp")
add_executable(unit_tests ${src})
target_link_libraries(unit_tests google-test ${BMALLOC_LIBS})
