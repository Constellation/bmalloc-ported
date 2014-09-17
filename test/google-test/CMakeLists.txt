# google test library

add_definitions("-Wno-unused-private-field")
add_library(google-test STATIC gtest-all.cc gtest_main.cc)
