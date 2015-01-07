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
#ifndef HELPER_ALLOCATOR_H_
#define HELPER_ALLOCATOR_H_
#include <cstddef>
#include <memory>
#include <new>
#include <utility>

template<class T, class AllocatorTraits>
class Allocator {
public:
    typedef T value_type;
    typedef T& reference;
    typedef const T& const_reference;
    typedef T* iterator;
    typedef const T* const_iterator;
    typedef T* pointer;
    typedef const T* const_pointer;
    typedef std::ptrdiff_t difference_type;
    typedef std::size_t size_type;

    typedef Allocator<T, AllocatorTraits> this_type;

    value_type* allocate(std::size_t n)
    {
        return static_cast<value_type*>(AllocatorTraits::malloc(sizeof(T) * n));
    }

    void deallocate(value_type* ptr, std::size_t)
    {
        AllocatorTraits::free(static_cast<void*>(ptr));
    }

    Allocator() noexcept { }
    Allocator(const Allocator&) noexcept { }
    template<class U> Allocator(const Allocator<U, AllocatorTraits>& ) noexcept { }

    template <class U>
    struct rebind {
        typedef Allocator<U, AllocatorTraits> other;
    };

    ~Allocator() noexcept { }

    template<class U> bool operator==(const Allocator<U, AllocatorTraits>&)
    {
        return true;
    }

    template<class U> bool operator!=(const Allocator<U, AllocatorTraits>&)
    {
        return false;
    }

    // For old GCCs.
    template<class U>
    void destroy(U* p)
    {
        p->~U();
    }

    template<class U, class... Args>
    void construct(U* p, Args&&... args)
    {
        ::new ((void*)p) U(std::forward<Args>(args)...);
    }
};

#endif  /* HELPER_ALLOCATOR_H_ */
