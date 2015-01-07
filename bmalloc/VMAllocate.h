/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef VMAllocate_h
#define VMAllocate_h

#include "BAssert.h"
#include "Range.h"
#include "Sizes.h"
#include "Syscall.h"
#include <algorithm>
#ifdef __APPLE__
#include <mach/vm_statistics.h>
#endif
#include <sys/mman.h>
#include <unistd.h>

namespace bmalloc {

#ifdef VM_MEMORY_TCMALLOC
#define BMALLOC_VM_TAG VM_MAKE_TAG(VM_MEMORY_TCMALLOC)
#else
#define BMALLOC_VM_TAG (MAP_PRIVATE | MAP_ANON)
#endif

inline size_t vmSize(size_t size)
{
    return roundUpToMultipleOf<vmPageSize>(size);
}

inline void vmValidate(size_t vmSize)
{
    // We use getpagesize() here instead of vmPageSize because vmPageSize is
    // allowed to be larger than the OS's true page size.

    UNUSED(vmSize);
    BASSERT(vmSize);
    BASSERT(vmSize == roundUpToMultipleOf(static_cast<size_t>(getpagesize()), vmSize));
}

inline void vmValidate(void* p, size_t vmSize)
{
    // We use getpagesize() here instead of vmPageSize because vmPageSize is
    // allowed to be larger than the OS's true page size.

    vmValidate(vmSize);
    
    UNUSED(p);
    BASSERT(p);
    BASSERT(p == mask(p, ~(getpagesize() - 1)));
}

inline void* vmAllocate(size_t vmSize)
{
    vmValidate(vmSize);
    void* result = mmap(0, vmSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, BMALLOC_VM_TAG, 0);
    RELEASE_BASSERT(result != MAP_FAILED);
    return result;
}

inline void vmDeallocate(void* p, size_t vmSize)
{
    vmValidate(p, vmSize);
    munmap(p, vmSize);
}

// Allocates vmSize bytes at a specified power-of-two alignment.
// Use this function to create maskable memory regions.

inline void* vmAllocate(size_t vmSize, size_t vmAlignment)
{
    vmValidate(vmSize);
    vmValidate(vmAlignment);

    size_t mappedSize = std::max(vmSize, vmAlignment) + vmAlignment;
    char* mapped = static_cast<char*>(vmAllocate(mappedSize));
    char* mappedEnd = mapped + mappedSize;

    char* aligned = roundUpToMultipleOf(vmAlignment, mapped);
    char* alignedEnd = aligned + vmSize;
    
    if (size_t leftExtra = aligned - mapped)
        vmDeallocate(mapped, leftExtra);
    
    if (size_t rightExtra = mappedEnd - alignedEnd)
        vmDeallocate(alignedEnd, rightExtra);

    return aligned;
}

inline void vmDeallocatePhysicalPages(void* p, size_t vmSize)
{
    vmValidate(p, vmSize);
#if defined(MADV_FREE_REUSABLE)
    SYSCALL(madvise(p, vmSize, MADV_FREE_REUSABLE));
#else
    SYSCALL(madvise(p, vmSize, MADV_DONTNEED));
#endif
}

inline void vmAllocatePhysicalPages(void* p, size_t vmSize)
{
    vmValidate(p, vmSize);
#if defined(MADV_FREE_REUSE)
    SYSCALL(madvise(p, vmSize, MADV_FREE_REUSE));
#else
    SYSCALL(madvise(p, vmSize, MADV_WILLNEED));
#endif
}

// Trims requests that are un-page-aligned. NOTE: size must be at least a page.
inline void vmDeallocatePhysicalPagesSloppy(void* p, size_t size)
{
    BASSERT(size >= vmPageSize);

    char* begin = roundUpToMultipleOf<vmPageSize>(static_cast<char*>(p));
    char* end = roundDownToMultipleOf<vmPageSize>(static_cast<char*>(p) + size);

    Range range(begin, end - begin);
    if (!range)
        return;
    vmDeallocatePhysicalPages(range.begin(), range.size());
}

// Expands requests that are un-page-aligned. NOTE: Allocation must proceed left-to-right.
inline void vmAllocatePhysicalPagesSloppy(void* p, size_t size)
{
    char* begin = roundUpToMultipleOf<vmPageSize>(static_cast<char*>(p));
    char* end = roundUpToMultipleOf<vmPageSize>(static_cast<char*>(p) + size);

    Range range(begin, end - begin);
    if (!range)
        return;
    vmAllocatePhysicalPages(range.begin(), range.size());
}

} // namespace bmalloc

#endif // VMAllocate_h
