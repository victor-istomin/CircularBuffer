#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include <array>
#include <ranges>
#include <algorithm>

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

    CHECK(std::distance(ints.findNthRecent(2), ints.cend()) == 2);
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

        CHECK(std::distance(ints.findNthRecent(2 * ints.size() + 1), ints.cend()) == clampCount(i));
        CHECK(std::distance(ints.findNthRecent(i), ints.cend())     == clampCount(i));
        CHECK(std::distance(ints.findNthRecent(i + 1), ints.cend()) == clampCount(i));
        CHECK(std::distance(ints.findNthRecent(i - 1), ints.cend()) == clampCount(i - 1));

        ints.pushBack(testArray[i]);

        if (ints.size() >= k_lastCheckSize)
        {
            int* lastArrPtr = &testArray[i + 1] - k_lastCheckSize;
            for (auto it = ints.findNthRecent(k_lastCheckSize); it != std::end(ints); ++it, ++lastArrPtr)
                CHECK(*lastArrPtr == *it);   
        }

        size_t wholeSize = ints.size();
        CHECK(std::distance(ints.findNthRecent(wholeSize),     ints.cend()) == wholeSize);
        CHECK(std::distance(ints.findNthRecent(wholeSize * 2), ints.cend()) == wholeSize);
        CHECK(std::distance(ints.findNthRecent(wholeSize / 2), ints.cend()) == wholeSize / 2);
    }

    CHECK(ints.size() == k_bufferSize);
    int* lastArrPtr = std::end(testArray) - k_lastCheckSize;
    for (auto it = ints.findNthRecent(k_lastCheckSize); it != std::end(ints); ++it, ++lastArrPtr)
        CHECK(*lastArrPtr == *it);
}

TEST_CASE("std::array based")
{
    int testArray[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    static constexpr int k_bufferSize = (int)std::size(testArray) / 2;
    static constexpr size_t k_lastCheckSize = 3;

    auto ints = CircularBuffer<int, std::array<int, k_bufferSize>>();

    for(int v : testArray)
        ints.pushBack(v);

    CHECK(ints.size() == k_bufferSize);
    CHECK(std::size(ints) == k_bufferSize);

    CHECK(std::distance(ints.findNthRecent(k_lastCheckSize), ints.cend()) == k_lastCheckSize);
    int* lastArrPtr = &testArray[ std::size(testArray) - k_lastCheckSize ];
    for(auto it = ints.findNthRecent(k_lastCheckSize); it != std::end(ints); ++it, ++lastArrPtr)
        CHECK(*lastArrPtr == *it);

    int correctBack  = testArray[std::size(testArray) - 1];
    int correctFront = testArray[std::size(testArray) - ints.size()];
    CHECK(ints.back() == correctBack);
    CHECK(ints.front() == correctFront);
}

struct Trackable
{
    int m_value  = 0;
    int m_copies = 0; 
    int m_moves  = 0;

    Trackable(int v = 0)              : m_value(v) {}
    Trackable(const Trackable& other) : m_value(other.m_value), m_copies(other.m_copies + 1), m_moves(other.m_moves) {}    
    Trackable(Trackable&& other)      : m_value(other.m_value), m_copies(other.m_copies), m_moves(other.m_moves + 1) {}

    Trackable& operator=(const Trackable& other)
    {
        m_value  = other.m_value;
        m_copies = other.m_copies + 1;
        m_moves  = other.m_moves; 
        return *this;
    }

    Trackable& operator=(Trackable&& other)
    {
        m_value  = other.m_value;
        m_copies = other.m_copies;
        m_moves  = other.m_moves + 1; 
        return *this;
    }
};

TEST_CASE("Trackable preconditions")
{
    Trackable a = 1;
    Trackable a2 = a;
    Trackable a3 = 3; a3 = a2;
    CHECK(a3.m_value == 1);
    CHECK(a3.m_copies == 2);
    CHECK(a3.m_moves == 0);

    Trackable b = 2;
    Trackable b2 = std::move(b);
    Trackable b3 = 0; b3 = std::move(b2);
    CHECK(b3.m_value == 2);
    CHECK(b3.m_copies == 0);
    CHECK(b3.m_moves == 2);
}

TEST_CASE("Copy and Move")
{
    constexpr int k_size = 3;
    using Buffer = CircularBuffer<Trackable, std::array<Trackable, k_size>>;

    // std::array is used so the buffer can't be moved and it will force items to move
    Buffer movedFrom;

    // no copy during the init
    for (int i = 0; i < k_size; ++i)
    {
        movedFrom.pushBack(i);           // pushBack(Trackable&&) <- Trackable(int) <- i 
        CHECK(movedFrom.back().m_value == i);
        CHECK(movedFrom.back().m_copies == 0);
        CHECK(movedFrom.back().m_moves == 1);
    }

    // no copy during the overwrite
    for (int i = 0; i < k_size; ++i)
    {
        int value = static_cast<int>(i + k_size);
        movedFrom.pushBack(value);                   // pushBack(Trackable&&) <- Trackable(int) <- i 
        CHECK(movedFrom.back().m_value == value);
        CHECK(movedFrom.back().m_copies == 0);
        CHECK(movedFrom.back().m_moves == 1);
    }

    // no copy during the movement of non-movable buffer
    auto movedTo = std::move(movedFrom);
    CHECK(movedTo.front().m_value == k_size);
    CHECK(movedTo.back().m_value == (k_size * 2 - 1));
    for(const Trackable& t : movedTo)
    {
        CHECK(t.m_copies == 0);
        CHECK(t.m_moves == 2);        
    }

    // copy counter check: movedFrom2 is a copy of movedTo -> copy occurs
    auto movedFrom2 = movedTo;
    CHECK(movedFrom2.front().m_value == k_size);
    CHECK(movedFrom2.back().m_value == (k_size * 2 - 1));
    for(const Trackable& t : movedFrom2)
    {
        CHECK(t.m_copies == 1);
        CHECK(t.m_moves == 2);        
    }

    // check move assignement
    movedTo = std::move(movedFrom2);
    const auto& movedToRef = movedTo;    // just trying to check the const-correctness
    CHECK(movedToRef.front().m_value == k_size);
    CHECK(movedToRef.back().m_value == (k_size * 2 - 1));
    for(const Trackable& t : movedToRef)
    {
        CHECK(t.m_copies == 1);
        CHECK(t.m_moves == 3);        
    }

    // check copy assignment
    Buffer copied;
    copied = movedTo;                   // copied to a temporary, then moved into 'copied'
    CHECK(copied.front().m_value == k_size);
    CHECK(copied.back().m_value == (k_size * 2 - 1));
    for (const Trackable& t : copied)
    {
        CHECK(t.m_copies == 2);
        CHECK(t.m_moves == 4);
    }
}

TEST_CASE("Copy to self")
{
    constexpr size_t k_size = 3;
    using Buffer = CircularBuffer<Trackable, std::array<Trackable, k_size>>;

    // std::array is used so the buffer can't be moved and it will force items to move
    Buffer buffer;

    // no copy during the init
    for (int i = 0; i < k_size; ++i)
    {
        buffer.pushBack(i);           // pushBack(Trackable&&) <- Trackable(int) <- i 
        CHECK(buffer.back().m_value == i);
        CHECK(buffer.back().m_copies == 0);
        CHECK(buffer.back().m_moves == 1);
    }

    const Buffer& cref = buffer;
    Buffer& ref = buffer;

    // still no copies or moves
    buffer = buffer = ref = cref;
    for (const Trackable& t : buffer)
    {
        CHECK(t.m_copies == 0);
        CHECK(t.m_moves == 1);
    }
}

TEST_CASE("Move to self")
{
    constexpr size_t k_size = 3;
    using Buffer = CircularBuffer<Trackable, std::array<Trackable, k_size>>;

    // std::array is used so the buffer can't be moved and it will force items to move
    Buffer buffer;

    // no copy during the init
    for (int i = 0; i < k_size; ++i)
    {
        buffer.pushBack(i);           // pushBack(Trackable&&) <- Trackable(int) <- i 
        CHECK(buffer.back().m_value == i);
        CHECK(buffer.back().m_copies == 0);
        CHECK(buffer.back().m_moves == 1);
    }

    // still no copies or moves
    Buffer& ref = buffer;
    buffer = std::move(ref);
    for (const Trackable& t : buffer)
    {
        CHECK(t.m_copies == 0);
        CHECK(t.m_moves == 1);
    }
}

TEST_CASE("most recent")
{
    using Buffer = CircularBuffer<int>;

    constexpr int k_size = 3;
    Buffer buffer = Buffer(k_size);
    std::vector<int> referenceVector;

    for(int i = 0; i < k_size + 1; ++i)  // intentional overflow
    {
        buffer.pushBack(i);
        referenceVector.push_back(i);

        constexpr int k_requestedItems = k_size - 1;
        const int availableItems = std::min(k_requestedItems, (int)buffer.size());
        auto itVectorTail = referenceVector.end() - availableItems;
        bool areTailEqual = std::equal(buffer.findNthRecent(k_requestedItems), buffer.cend(), itVectorTail, referenceVector.end());
        CHECK(areTailEqual);
    }
}

TEST_CASE("ranges: static asserts")
{
    using Buffer   = CircularBuffer<int>;
    using Iterator = CircularBuffer<int>::iterator;

    static_assert(std::bidirectional_iterator<Iterator>, "CircularBuffer::iterator should satisfy the bidirectional_iterator concept");
    static_assert(std::ranges::bidirectional_range<Buffer>, "CircularBuffer is a bidirectional_range");
}

TEST_CASE("ranges: N-th recent")
{
    using Buffer = CircularBuffer<int>;
    namespace views = std::ranges::views;

    constexpr int k_size = 3;
    Buffer buffer = Buffer(k_size);
    std::vector<int> referenceVector;

    for (int i = 0; i < k_size + 1; ++i)  // intentional overflow
    {
        buffer.pushBack(i);
        referenceVector.push_back(i);

        constexpr int k_requestedItems = k_size - 1;
        const int availableItems = std::min(k_requestedItems, (int)buffer.size());

        // check that std::ranges could be used to get most recent N elements
        auto vectorTail = referenceVector | views::reverse | views::take(availableItems) | views::reverse;
        auto bufferTail = buffer | views::reverse | views::take(availableItems) | views::reverse;
        bool areTailEqual = std::ranges::equal(vectorTail, bufferTail);
        CHECK(areTailEqual);

        // also check 'buffer.mostRecent' shortcut
        auto mostRecentRange = std::ranges::subrange(buffer.findNthRecent(k_requestedItems), buffer.end());
        bool areMostRecentEqual = std::ranges::equal(vectorTail, mostRecentRange);
        CHECK(areMostRecentEqual);
    }
}

TEST_CASE("ranges: most recent")
{
    using Buffer = CircularBuffer<int>;
    namespace views = std::ranges::views;

    constexpr int k_size = 3;
    constexpr int k_requestedItems = k_size - 1;
    
    Buffer buffer = Buffer(k_size);
    std::vector<int> referenceVector;

    for (int i = 0; i < k_size + 1; ++i)  // intentional overflow
    {
        buffer.pushBack(i);
        referenceVector.push_back(i);

        const int availableItems = std::min(k_requestedItems, (int)buffer.size());

        // check that buffer.mostRecent() could be used to get most recent N elements
        auto vectorTail = referenceVector | views::reverse | views::take(availableItems) | views::reverse;
        auto mostRecentRange = buffer.mostRecent(k_requestedItems);
        bool areMostRecentEqual = std::ranges::equal(vectorTail, mostRecentRange);
        CHECK(areMostRecentEqual);
    }

    // check that returned range is reversible
    auto vectorTailReversed = referenceVector | views::reverse | views::take(k_requestedItems);
    auto mostRecentReversed = buffer.mostRecent(k_requestedItems) | views::reverse;
    bool areReversedEqual = std::ranges::equal(vectorTailReversed, mostRecentReversed);
    CHECK(areReversedEqual);
}

TEST_CASE("ranges: mostRecent and rvalue")
{
    using Buffer = CircularBuffer<int>;
    namespace views = std::ranges::views;

    constexpr int k_size = 3;
    constexpr int k_requestedItems = k_size - 1;

    auto makeBuffer = [](int size)
    {
        Buffer buffer = Buffer(size);
        for (int i = 0; i < size; ++i)
            buffer.pushBack(i);
        return buffer;
    };

    // this one intentionally does not compile
    //auto mostRecent = makeBuffer(k_size).mostRecent(k_requestedItems);
    const Buffer& buffer = makeBuffer(k_size);
    auto mostRecent = buffer.mostRecent(k_requestedItems);
    bool areEqual = std::ranges::equal(mostRecent, std::vector<int>{1, 2});
    CHECK(areEqual);
}
