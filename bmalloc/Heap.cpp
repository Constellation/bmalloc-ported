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

#include "BoundaryTagInlines.h"
#include "Heap.h"
#include "LargeChunk.h"
#include "Line.h"
#include "MediumChunk.h"
#include "Page.h"
#include "PerProcess.h"
#include "SmallChunk.h"
#include "XLargeChunk.h"
#include <thread>

namespace bmalloc {

static inline void sleep(std::unique_lock<StaticMutex>& lock, std::chrono::milliseconds duration)
{
    if (duration == std::chrono::milliseconds(0))
        return;
    
    lock.unlock();
    std::this_thread::sleep_for(duration);
    lock.lock();
}

Heap::Heap(std::lock_guard<StaticMutex>&)
    : m_isAllocatingPages(false)
    , m_scavenger(*this, &Heap::concurrentScavenge)
{
    initializeLineMetadata();
}

void Heap::initializeLineMetadata()
{
    for (unsigned short size = alignment; size <= smallMax; size += alignment) {
        unsigned short startOffset = 0;
        for (size_t lineNumber = 0; lineNumber < SmallPage::lineCount - 1; ++lineNumber) {
            unsigned short objectCount;
            unsigned short remainder;
            divideRoundingUp(static_cast<unsigned short>(SmallPage::lineSize - startOffset), size, objectCount, remainder);
            BASSERT(objectCount);
            m_smallLineMetadata[sizeClass(size)][lineNumber] = { startOffset, objectCount };
            startOffset = remainder ? size - remainder : 0;
        }

        // The last line in the page rounds down instead of up because it's not allowed to overlap into its neighbor.
        unsigned short objectCount = static_cast<unsigned short>((SmallPage::lineSize - startOffset) / size);
        m_smallLineMetadata[sizeClass(size)][SmallPage::lineCount - 1] = { startOffset, objectCount };
    }

    for (unsigned short size = smallMax + alignment; size <= mediumMax; size += alignment) {
        unsigned short startOffset = 0;
        for (size_t lineNumber = 0; lineNumber < MediumPage::lineCount - 1; ++lineNumber) {
            unsigned short objectCount;
            unsigned short remainder;
            divideRoundingUp(static_cast<unsigned short>(MediumPage::lineSize - startOffset), size, objectCount, remainder);
            BASSERT(objectCount);
            m_mediumLineMetadata[sizeClass(size)][lineNumber] = { startOffset, objectCount };
            startOffset = remainder ? size - remainder : 0;
        }

        // The last line in the page rounds down instead of up because it's not allowed to overlap into its neighbor.
        unsigned short objectCount = static_cast<unsigned short>((MediumPage::lineSize - startOffset) / size);
        m_mediumLineMetadata[sizeClass(size)][MediumPage::lineCount - 1] = { startOffset, objectCount };
    }
}

void Heap::concurrentScavenge()
{
    std::unique_lock<StaticMutex> lock(PerProcess<Heap>::mutex());
    scavenge(lock, scavengeSleepDuration);
}
    
void Heap::scavenge(std::unique_lock<StaticMutex>& lock, std::chrono::milliseconds sleepDuration)
{
    scavengeSmallPages(lock, sleepDuration);
    scavengeMediumPages(lock, sleepDuration);
    scavengeLargeRanges(lock, sleepDuration);

    sleep(lock, sleepDuration);
}

void Heap::scavengeSmallPages(std::unique_lock<StaticMutex>& lock, std::chrono::milliseconds sleepDuration)
{
    while (1) {
        if (m_isAllocatingPages) {
            m_isAllocatingPages = false;

            sleep(lock, sleepDuration);
            continue;
        }

        if (!m_smallPages.size())
            return;
        m_vmHeap.deallocateSmallPage(lock, m_smallPages.pop());
    }
}

void Heap::scavengeMediumPages(std::unique_lock<StaticMutex>& lock, std::chrono::milliseconds sleepDuration)
{
    while (1) {
        if (m_isAllocatingPages) {
            m_isAllocatingPages = false;

            sleep(lock, sleepDuration);
            continue;
        }

        if (!m_mediumPages.size())
            return;
        m_vmHeap.deallocateMediumPage(lock, m_mediumPages.pop());
    }
}

void Heap::scavengeLargeRanges(std::unique_lock<StaticMutex>& lock, std::chrono::milliseconds sleepDuration)
{
    while (1) {
        if (m_isAllocatingPages) {
            m_isAllocatingPages = false;

            sleep(lock, sleepDuration);
            continue;
        }

        Range range = m_largeRanges.takeGreedy(vmPageSize);
        if (!range)
            return;
        m_vmHeap.deallocateLargeRange(lock, range);
    }
}

void Heap::refillSmallBumpRangeCache(std::lock_guard<StaticMutex>& lock, size_t sizeClass, SmallBumpRangeCache& rangeCache)
{
    BASSERT(!rangeCache.size());
    SmallPage* page = allocateSmallPage(lock, sizeClass);
    SmallLine* lines = page->begin();

    // Due to overlap from the previous line, the last line in the page may not be able to fit any objects.
    size_t end = SmallPage::lineCount;
    if (!m_smallLineMetadata[sizeClass][SmallPage::lineCount - 1].objectCount)
        --end;

    // Find a free line.
    for (size_t lineNumber = 0; lineNumber < end; ++lineNumber) {
        if (lines[lineNumber].refCount(lock))
            continue;

        LineMetadata& lineMetadata = m_smallLineMetadata[sizeClass][lineNumber];
        char* begin = lines[lineNumber].begin() + lineMetadata.startOffset;
        unsigned short objectCount = lineMetadata.objectCount;
        lines[lineNumber].ref(lock, lineMetadata.objectCount);
        page->ref(lock);

        // Merge with subsequent free lines.
        while (++lineNumber < end) {
            if (lines[lineNumber].refCount(lock))
                break;

            LineMetadata& lineMetadata = m_smallLineMetadata[sizeClass][lineNumber];
            objectCount += lineMetadata.objectCount;
            lines[lineNumber].ref(lock, lineMetadata.objectCount);
            page->ref(lock);
        }

        rangeCache.push({ begin, objectCount });
    }
}

void Heap::refillMediumBumpRangeCache(std::lock_guard<StaticMutex>& lock, size_t sizeClass, MediumBumpRangeCache& rangeCache)
{
    MediumPage* page = allocateMediumPage(lock, sizeClass);
    BASSERT(!rangeCache.size());
    MediumLine* lines = page->begin();

    // Due to overlap from the previous line, the last line in the page may not be able to fit any objects.
    size_t end = MediumPage::lineCount;
    if (!m_mediumLineMetadata[sizeClass][MediumPage::lineCount - 1].objectCount)
        --end;

    // Find a free line.
    for (size_t lineNumber = 0; lineNumber < end; ++lineNumber) {
        if (lines[lineNumber].refCount(lock))
            continue;

        LineMetadata& lineMetadata = m_mediumLineMetadata[sizeClass][lineNumber];
        char* begin = lines[lineNumber].begin() + lineMetadata.startOffset;
        unsigned short objectCount = lineMetadata.objectCount;
        lines[lineNumber].ref(lock, lineMetadata.objectCount);
        page->ref(lock);
        
        // Merge with subsequent free lines.
        while (++lineNumber < end) {
            if (lines[lineNumber].refCount(lock))
                break;

            LineMetadata& lineMetadata = m_mediumLineMetadata[sizeClass][lineNumber];
            objectCount += lineMetadata.objectCount;
            lines[lineNumber].ref(lock, lineMetadata.objectCount);
            page->ref(lock);
        }

        rangeCache.push({ begin, objectCount });
    }
}

SmallPage* Heap::allocateSmallPage(std::lock_guard<StaticMutex>& lock, size_t sizeClass)
{
    Vector<SmallPage*>& smallPagesWithFreeLines = m_smallPagesWithFreeLines[sizeClass];
    while (smallPagesWithFreeLines.size()) {
        SmallPage* page = smallPagesWithFreeLines.pop();
        if (!page->refCount(lock) || page->sizeClass() != sizeClass) // Page was promoted to the pages list.
            continue;
        return page;
    }
    
    m_isAllocatingPages = true;

    SmallPage* page = [this, sizeClass]() {
        if (m_smallPages.size())
            return m_smallPages.pop();
        
        SmallPage* page = m_vmHeap.allocateSmallPage();
        vmAllocatePhysicalPages(page->begin()->begin(), vmPageSize);
        return page;
    }();

    page->setSizeClass(sizeClass);
    return page;
}

MediumPage* Heap::allocateMediumPage(std::lock_guard<StaticMutex>& lock, size_t sizeClass)
{
    Vector<MediumPage*>& mediumPagesWithFreeLines = m_mediumPagesWithFreeLines[sizeClass];
    while (mediumPagesWithFreeLines.size()) {
        MediumPage* page = mediumPagesWithFreeLines.pop();
        if (!page->refCount(lock) || page->sizeClass() != sizeClass) // Page was promoted to the pages list.
            continue;
        return page;
    }
    
    m_isAllocatingPages = true;

    MediumPage* page = [this, sizeClass]() {
        if (m_mediumPages.size())
            return m_mediumPages.pop();
        
        MediumPage* page = m_vmHeap.allocateMediumPage();
        vmAllocatePhysicalPages(page->begin()->begin(), vmPageSize);
        return page;
    }();

    page->setSizeClass(sizeClass);
    return page;
}

void Heap::deallocateSmallLine(std::lock_guard<StaticMutex>& lock, SmallLine* line)
{
    BASSERT(!line->refCount(lock));
    SmallPage* page = SmallPage::get(line);
    size_t refCount = page->refCount(lock);
    page->deref(lock);

    switch (refCount) {
    case SmallPage::lineCount: {
        // First free line in the page.
        m_smallPagesWithFreeLines[page->sizeClass()].push(page);
        break;
    }
    case 1: {
        // Last free line in the page.
        m_smallPages.push(page);
        m_scavenger.run();
        break;
    }
    }
}

void Heap::deallocateMediumLine(std::lock_guard<StaticMutex>& lock, MediumLine* line)
{
    BASSERT(!line->refCount(lock));
    MediumPage* page = MediumPage::get(line);
    size_t refCount = page->refCount(lock);
    page->deref(lock);

    switch (refCount) {
    case MediumPage::lineCount: {
        // First free line in the page.
        m_mediumPagesWithFreeLines[page->sizeClass()].push(page);
        break;
    }
    case 1: {
        // Last free line in the page.
        m_mediumPages.push(page);
        m_scavenger.run();
        break;
    }
    }
}

void* Heap::allocateXLarge(std::lock_guard<StaticMutex>&, size_t size)
{
    XLargeChunk* chunk = XLargeChunk::create(size);

    BeginTag* beginTag = LargeChunk::beginTag(chunk->begin());
    beginTag->setXLarge();
    beginTag->setFree(false);
    beginTag->setHasPhysicalPages(true);
    
    return chunk->begin();
}

void Heap::deallocateXLarge(std::lock_guard<StaticMutex>&, void* object)
{
    XLargeChunk* chunk = XLargeChunk::get(object);
    XLargeChunk::destroy(chunk);
}

void* Heap::allocateLarge(std::lock_guard<StaticMutex>&, size_t size)
{
    BASSERT(size <= largeMax);
    BASSERT(size >= largeMin);
    
    m_isAllocatingPages = true;

    Range range = m_largeRanges.take(size);
    if (!range)
        range = m_vmHeap.allocateLargeRange(size);
    
    Range leftover;
    bool hasPhysicalPages;
    BoundaryTag::allocate(size, range, leftover, hasPhysicalPages);

    if (!!leftover)
        m_largeRanges.insert(leftover);
    
    if (!hasPhysicalPages)
        vmAllocatePhysicalPagesSloppy(range.begin(), range.size());

    return range.begin();
}

void Heap::deallocateLarge(std::lock_guard<StaticMutex>&, void* object)
{
    Range range = BoundaryTag::deallocate(object);
    m_largeRanges.insert(range);
    m_scavenger.run();
}

} // namespace bmalloc
