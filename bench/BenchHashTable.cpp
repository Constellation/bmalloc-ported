/*
  Copyright (C) 2014 Yusuke Suzuki <utatane.tea@gmail.com>

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <benchmark/benchmark.h>

#include <unordered_map>
#include <utility>
#include <helper/API.h>
#include <helper/BAllocator.h>


void HashTable_Insert_BMalloc(benchmark::State& state) {
    typedef int Key;
    typedef int Value;
    typedef std::unordered_map<Key, Value, std::hash<Key>, std::equal_to<Key>, BAllocator<std::pair<const Key, Value>>> HashTable;
    while (state.KeepRunning()) {
        HashTable table;
        for (int i = 0; i < state.range_x(); ++i) {
            table.insert(std::make_pair(i, 20));
        }
    }
}
BENCHMARK(HashTable_Insert_BMalloc)->Arg(1 << 14)->Arg(1 << 15)->Arg(1 << 16)->Arg(1 << 17);

void HashTable_Insert_System(benchmark::State& state) {
    typedef int Key;
    typedef int Value;
    typedef std::unordered_map<Key, Value, std::hash<Key>, std::equal_to<Key>, std::allocator<std::pair<const Key, Value>>> HashTable;
    while (state.KeepRunning()) {
        HashTable table;
        for (int i = 0; i < state.range_x(); ++i) {
            table.insert(std::make_pair(i, 20));
        }
    }
}
BENCHMARK(HashTable_Insert_System)->Arg(1 << 14)->Arg(1 << 15)->Arg(1 << 16)->Arg(1 << 17);
