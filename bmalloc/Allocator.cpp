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

#include "Allocator.h"
#include "BAssert.h"
#include "Deallocator.h"
#include "Heap.h"
#include "PerProcess.h"
#include "Sizes.h"
#include <algorithm>

using namespace std;

namespace bmalloc {

Allocator::Allocator(Deallocator& deallocator)
    : m_deallocator(deallocator)
{
    for (unsigned short size = alignment; size <= mediumMax; size += alignment)
        m_bumpAllocators[sizeClass(size)].init(size);
}

Allocator::~Allocator()
{
    scavenge();
}

void Allocator::scavenge()
{
    for (unsigned short i = alignment; i <= mediumMax; i += alignment) {
        BumpAllocator& allocator = m_bumpAllocators[sizeClass(i)];
        BumpRangeCache& bumpRangeCache = m_bumpRangeCaches[sizeClass(i)];

        while (allocator.canAllocate())
            m_deallocator.deallocate(allocator.allocate());

        while (bumpRangeCache.size()) {
            allocator.refill(bumpRangeCache.pop());
            while (allocator.canAllocate())
                m_deallocator.deallocate(allocator.allocate());
        }

        allocator.clear();
    }
}

NO_INLINE BumpRange Allocator::allocateBumpRangeSlowCase(size_t sizeClass)
{
    BumpRangeCache& bumpRangeCache = m_bumpRangeCaches[sizeClass];

    std::lock_guard<StaticMutex> lock(PerProcess<Heap>::mutex());
    if (sizeClass <= bmalloc::sizeClass(smallMax))
        PerProcess<Heap>::getFastCase()->refillSmallBumpRangeCache(lock, sizeClass, bumpRangeCache);
    else
        PerProcess<Heap>::getFastCase()->refillMediumBumpRangeCache(lock, sizeClass, bumpRangeCache);

    return bumpRangeCache.pop();
}

INLINE BumpRange Allocator::allocateBumpRange(size_t sizeClass)
{
    BumpRangeCache& bumpRangeCache = m_bumpRangeCaches[sizeClass];
    if (!bumpRangeCache.size())
        return allocateBumpRangeSlowCase(sizeClass);
    return bumpRangeCache.pop();
}

NO_INLINE void* Allocator::allocateLarge(size_t size)
{
    size = roundUpToMultipleOf<largeAlignment>(size);
    std::lock_guard<StaticMutex> lock(PerProcess<Heap>::mutex());
    return PerProcess<Heap>::getFastCase()->allocateLarge(lock, size);
}

NO_INLINE void* Allocator::allocateXLarge(size_t size)
{
    size = roundUpToMultipleOf<largeAlignment>(size);
    std::lock_guard<StaticMutex> lock(PerProcess<Heap>::mutex());
    return PerProcess<Heap>::getFastCase()->allocateXLarge(lock, size);
}

void* Allocator::allocateSlowCase(size_t size)
{
    if (size <= mediumMax) {
        size_t sizeClass = bmalloc::sizeClass(size);
        BumpAllocator& allocator = m_bumpAllocators[sizeClass];
        allocator.refill(allocateBumpRange(sizeClass));
        return allocator.allocate();
    }

    if (size <= largeMax)
        return allocateLarge(size);

    return allocateXLarge(size);
}

} // namespace bmalloc
