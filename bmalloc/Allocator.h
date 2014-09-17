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

#ifndef Allocator_h
#define Allocator_h

#include "BumpAllocator.h"
#include "FixedVector.h"
#include "MediumAllocator.h"
#include "Sizes.h"
#include "SmallLine.h"
#include <array>

namespace bmalloc {

class Deallocator;

// Per-cache object allocator.

class Allocator {
public:
    Allocator(Deallocator&);
    ~Allocator();

    void* allocate(size_t);
    bool allocateFastCase(size_t, void*&);
    void* allocateSlowCase(size_t);
    
    void scavenge();

private:
    typedef FixedVector<SmallLine*, smallLineCacheCapacity> SmallLineCache;
    typedef FixedVector<MediumLine*, mediumLineCacheCapacity> MediumLineCache;

    void* allocateFastCase(BumpAllocator&);

    void* allocateMedium(size_t);
    void* allocateLarge(size_t);
    void* allocateXLarge(size_t);
    
    SmallLine* allocateSmallLine(size_t smallSizeClass);
    MediumLine* allocateMediumLine();
    
    Deallocator& m_deallocator;

    std::array<BumpAllocator, smallMax / alignment> m_smallAllocators;
    std::array<BumpAllocator, mediumMax / alignment> m_mediumAllocators;

    std::array<SmallLineCache, smallMax / alignment> m_smallLineCaches;
    MediumLineCache m_mediumLineCache;
};

inline bool Allocator::allocateFastCase(size_t size, void*& object)
{
    if (size > smallMax)
        return false;

    BumpAllocator& allocator = m_smallAllocators[smallSizeClassFor(size)];
    if (!allocator.canAllocate())
        return false;

    object = allocator.allocate();
    return true;
}

inline void* Allocator::allocate(size_t size)
{
    void* object;
    if (!allocateFastCase(size, object))
        return allocateSlowCase(size);
    return object;
}

} // namespace bmalloc

#endif // Allocator_h
