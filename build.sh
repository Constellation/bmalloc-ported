cmake -H. -Brelease -DCMAKE_BUILD_TYPE=Release -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
ninja -C release
