#include <cmath>
#include <stdlib.h>
#include "VirtualMemory.h"
#include "PhysicalMemory.h"

struct traverser
{
    uint64_t entry_pagenum;
    uint64_t potential_pagenum;
    uint64_t tracker;
    uint64_t max_dist;
    uint64_t prev;
    word_t max_cyclic;
    uint64_t max_cyclic_parent;
    word_t max_frame;
    word_t empty_frame;
    uint64_t empty_parent;
    word_t caller;
};

void clearTable(uint64_t frameIndex)
{
    for (uint64_t i = 0; i < PAGE_SIZE; ++i)
    {
        PMwrite(frameIndex * PAGE_SIZE + i, 0);
    }
}

void VMinitialize()
{
    clearTable(0);
}

uint64_t calc_cyclic_min(uint64_t pageNumber, uint64_t otherPageNumber)
{
    uint64_t absl = 0;
    if (pageNumber > otherPageNumber) absl = pageNumber - otherPageNumber;
    else absl = otherPageNumber - pageNumber;
    if ((NUM_PAGES - absl) < absl) return (NUM_PAGES - absl);
    return absl;
}

void search_and_evict_h(struct traverser& t, word_t base, uint64_t height)
{
    if(height == 0)
    {
        uint64_t cyclic_dist = calc_cyclic_min(t.entry_pagenum, t.tracker);
        if(cyclic_dist > t.max_dist)
        {
            t.max_cyclic = base;
            t.max_dist = cyclic_dist;
            t.potential_pagenum = t.tracker;
            t.max_cyclic_parent = t.prev;
        }
        return;
    }
    bool empty = true;
    for(int i = 0; i < PAGE_SIZE; ++i)
    {
        word_t potential;
        PMread((uint64_t)((base * PAGE_SIZE) + i), &potential);
        if(potential != 0)
        {
            empty = false;
            t.tracker += i;
            if (height != 1)
            {
                t.tracker <<= OFFSET_WIDTH;
            }
            t.prev = (base * PAGE_SIZE) + i;
            search_and_evict_h(t, potential, height - 1);
        }
        if(potential > t.max_frame)
        {
            t.max_frame = potential;
        }
    }
    if (empty)
    {
        if(base != t.caller)
        {
            t.empty_frame = base;
            t.empty_parent = t.prev;
        }
    }
    t.tracker >>= OFFSET_WIDTH;
}


word_t search_and_evict(struct traverser& t)
{
    search_and_evict_h(t, 0, TABLES_DEPTH);
    if (t.max_frame < NUM_FRAMES - 1)
    {
        clearTable((uint64_t)t.max_frame + 1);
        return t.max_frame + 1;
    }
    if(t.empty_frame != 0)
    {
        PMwrite((uint64_t)t.empty_parent, 0);
        return t.empty_frame;
    }
    PMevict((uint64_t)t.max_cyclic, t.potential_pagenum);
    PMwrite((uint64_t)t.max_cyclic_parent , 0);
    clearTable((uint64_t)t.max_cyclic);
    return t.max_cyclic;
}

word_t get_physical(uint64_t& virtualAddress)
{
    int cur_height = TABLES_DEPTH;
    word_t prev_base = 0;
    word_t base_addr = 0;
    while(cur_height > 0)
    {
        uint64_t offset = get_offset(virtualAddress, (uint64_t)cur_height);
        prev_base = base_addr;
        PMread((uint64_t)(base_addr * PAGE_SIZE) + offset, &base_addr);
        if (base_addr == 0)
        {
            struct traverser t = {0, 0, 0, 0, 0, 0, 0};
            t.entry_pagenum = virtualAddress >> OFFSET_WIDTH;
            t.caller = prev_base;
            word_t new_frame = search_and_evict(t);
            PMwrite((uint64_t)(prev_base * PAGE_SIZE) + offset, new_frame);
            base_addr = new_frame;
            if (cur_height == 1)
            {
                PMrestore((uint64_t)new_frame, t.entry_pagenum);
            }
        }
        cur_height--;
    }
    return base_addr;
}

uint64_t get_offset(uint64_t& address, uint64_t level)
{
    uint64_t ret = address;
    uint64_t mod = address;
    uint64_t loop = (level + 1) * OFFSET_WIDTH;
    mod >>= loop;
    mod <<= loop;
    ret -= mod;
    loop -= OFFSET_WIDTH;
    ret >>= loop;
    return ret;
}


int VMread(uint64_t virtualAddress, word_t* value)
{
    if(virtualAddress > (pow(2, VIRTUAL_ADDRESS_WIDTH) - 1))
    {
        return 0;
    }
    uint64_t offset = get_offset(virtualAddress, 0);
    word_t address = get_physical(virtualAddress);
    PMread((address * PAGE_SIZE) + offset, value);
    return 1;
}


int VMwrite(uint64_t virtualAddress, word_t value)
{
    if(virtualAddress > (pow(2, VIRTUAL_ADDRESS_WIDTH) - 1))
    {
        return 0;
    }
    uint64_t offset = get_offset(virtualAddress, 0);
    word_t address = get_physical(virtualAddress);
    PMwrite((address * PAGE_SIZE) + offset, value);
    return 1;
}
