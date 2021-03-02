#include "VirtualMemory.h"

#include <cstdio>
#include <cassert>
#include <iostream>
#include <algorithm>
#include <chrono>

constexpr auto up_bound = VIRTUAL_MEMORY_SIZE - 1;

void random_test();

void simple_test();

void manual_test();

int main(int argc, char **argv)
{
//    manual_test();
//    simple_test();
    random_test();
    return 0;
}

void manual_test()
{
    /** ENTER CODE HERE **/
    VMinitialize();
    uint64_t addr = 0;
    VMwrite(5 * addr * PAGE_SIZE, (word_t) addr);
    word_t value;
    VMread(5 * addr * PAGE_SIZE, &value);

    std::cout << "Read from " << 5 * addr << " value: " << value << "\n";
    assert(uint64_t(value) == addr);
}

void random_test()
{
    VMinitialize();
    uint64_t j = 0;
    std::vector<uint64_t> addresses(up_bound);
    // Fills the vector with the numbers 0 through up_bound.
    std::generate(addresses.begin(), addresses.end(), [&] { return j++; });

    // Choose a random order for the read & write operations.
    std::vector<uint64_t> write_read_order(addresses);
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(write_read_order.begin(), write_read_order.end(), g);

    for (uint64_t address : write_read_order)
    {
        VMwrite(address, (word_t) address);
//        std::cout << "Writing to addr: " << address << " value: " << address << "\n";
    }

    word_t value;
    for (uint64_t address : write_read_order)
    {
        VMread(address, &value);
//        std::cout << "Expecting addr: " << address << " contains value: " << address << "\n";
        assert(word_t(address) == value);
    }
    std::cout << "random test succeeded.\n";
}

void simple_test()
{
    VMinitialize();
    for (uint64_t i = 0; i < (2 * NUM_FRAMES); ++i)
    {
        printf("writing to %lu\n", 5 * i);
        VMwrite(5 * i * PAGE_SIZE, (word_t) i);
    }

    for (uint64_t i = 0; i < (2 * NUM_FRAMES); ++i)
    {
        word_t value;
        VMread(5 * i * PAGE_SIZE, &value);

        printf("reading from %lu %d\n", 5 * i, value);
        assert(uint64_t(value) == i);
    }
    printf("simple test succeeded.\n");
}