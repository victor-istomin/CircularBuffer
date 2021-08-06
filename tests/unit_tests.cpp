#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "circularBuffer.hpp"

TEST_CASE("tesing circular data overwriting")
{
    static constexpr int k_bufferSize = 4;
    CircularBuffer<int> ints = CircularBuffer<int>(k_bufferSize);

    CHECK(ints.size() == 0);
    
    int testArray[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };
    for(int i = 0; i < (int)std::size(testArray); ++i)
    {
        int correctSize  = std::min(i + 1, k_bufferSize);
        int correctBack  = testArray[i];
        int correctFront = testArray[std::max(0, i + 1 - k_bufferSize)];

        ints.pushBack(correctBack);
        CHECK(ints.size()  == correctSize);
        CHECK(ints.back()  == correctBack);
        CHECK(ints.front() == correctFront);
    }
}

TEST_CASE("Iteration")
{
    int testArray[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };
    static constexpr int k_bufferSize = (int)std::size(testArray) / 2;
    CircularBuffer<int> ints = CircularBuffer<int>(k_bufferSize);

    for(int v : testArray)
    {
        ints.pushBack(v);

        const size_t pushedAmount = ints.size();

        size_t iteratedAmount = 0;
        for(auto it = ints.begin(); it != ints.end(); ++it)
            ++iteratedAmount;
        CHECK(iteratedAmount == pushedAmount);

        iteratedAmount = 0;
        for(auto it = std::prev(ints.end()); it != std::prev(ints.begin()); --it)
            ++iteratedAmount;
        CHECK(iteratedAmount == pushedAmount);
    }

    for (int i = 0; i < k_bufferSize; ++i)
    {
        ints.popFront();
        const size_t elementsLeft = ints.size();

        size_t iteratedAmount = 0;
        for(auto it = ints.begin(); it != ints.end(); ++it)
            ++iteratedAmount;
        CHECK(iteratedAmount == elementsLeft);

        iteratedAmount = 0;
        for(auto it = std::prev(ints.end()); it != std::prev(ints.begin()); --it)
            ++iteratedAmount;
        CHECK(iteratedAmount == elementsLeft);
    }
}

TEST_CASE("popFront() empty buffer")
{
    static constexpr int k_bufferSize = 4;
    CircularBuffer<int> ints = CircularBuffer<int>(k_bufferSize);

    // pop from empty buffer
    CHECK(ints.size() == 0);
    ints.popFront();
    CHECK(ints.size() == 0);
}

TEST_CASE("tesing std compatibility (distance, std:size)")
{
    int testArray[] = { 1, 2, 3, 4, 5, 6, 7 };
    static constexpr int k_bufferSize = (int)std::size(testArray);
    CircularBuffer<int> ints = CircularBuffer<int>(k_bufferSize);
    for (int v : testArray)
        ints.pushBack(v);

    CHECK(ints.size()     == k_bufferSize);
    CHECK(std::size(ints) == k_bufferSize);

    CHECK(std::distance(ints.mostRecent(2), ints.cend()) == 2);
}

TEST_CASE("CircularBuffer::mostRecent()")
{
    int testArray[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    static constexpr int k_bufferSize = (int)std::size(testArray) / 2;
    static constexpr size_t k_lastCheckSize = 3;
    CircularBuffer<int> ints = CircularBuffer<int>(k_bufferSize);

    for (int i = 0; i < (int)std::size(testArray); ++i)
    {
        auto clampCount = [&](int count) { return std::clamp<int>(count, 0, (int)ints.size()); };

        CHECK(std::distance(ints.mostRecent(2 * ints.size() + 1), ints.cend()) == clampCount(i));
        CHECK(std::distance(ints.mostRecent(i), ints.cend())     == clampCount(i));
        CHECK(std::distance(ints.mostRecent(i + 1), ints.cend()) == clampCount(i));
        CHECK(std::distance(ints.mostRecent(i - 1), ints.cend()) == clampCount(i - 1));

        ints.pushBack(testArray[i]);

        if (ints.size() >= k_lastCheckSize)
        {
            int* lastArrPtr = &testArray[i + 1] - k_lastCheckSize;
            for (auto it = ints.mostRecent(k_lastCheckSize); it != std::end(ints); ++it, ++lastArrPtr)
                CHECK(*lastArrPtr == *it);   
        }

        size_t wholeSize = ints.size();
        CHECK(std::distance(ints.mostRecent(wholeSize),     ints.cend()) == wholeSize);
        CHECK(std::distance(ints.mostRecent(wholeSize * 2), ints.cend()) == wholeSize);
        CHECK(std::distance(ints.mostRecent(wholeSize / 2), ints.cend()) == wholeSize / 2);
    }

    CHECK(ints.size() == k_bufferSize);
    int* lastArrPtr = std::end(testArray) - k_lastCheckSize;
    for (auto it = ints.mostRecent(k_lastCheckSize); it != std::end(ints); ++it, ++lastArrPtr)
        CHECK(*lastArrPtr == *it);
}
