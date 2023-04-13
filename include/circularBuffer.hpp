#pragma once

#include <iterator>
#include <vector>

#include <cstddef>      // ptrdiff_t
#include <type_traits>
#include <cassert>

#include <ranges>   // for subrange

// adds begin/end/size functions to its 'Derived' subclass 
// assumes that there is getUnderlyingType() method returns a reference to the underlying class that supports std::begin/end/size, 
// e.g. 'std::vector<T>& getUnderlyingType();'
// will be much easier once 'deducing this' is available
template <typename Derived> struct StdFunctionsMixin
{
    auto begin() { return std::begin(static_cast<Derived*>(this)->getUnderlyingType()); }
    auto begin() const { return std::begin(static_cast<const Derived*>(this)->getUnderlyingType()); }
    auto cbegin() const { return std::cbegin(static_cast<const Derived*>(this)->getUnderlyingType()); }

    auto end() { return std::end(static_cast<Derived*>(this)->getUnderlyingType()); }
    auto end() const { return std::end(static_cast<const Derived*>(this)->getUnderlyingType()); }
    auto cend() const { return std::cend(static_cast<const Derived*>(this)->getUnderlyingType()); }

    auto size() const { return std::size(static_cast<const Derived*>(this)->getUnderlyingType()); }
};

template <typename Type, size_t Size>
class ConstexprSizeBuffer
    : public StdFunctionsMixin< ConstexprSizeBuffer<Type, Size> >
{
    using ArrayType = std::array<Type, Size + 1/*sentinel at the end*/>;
    ArrayType m_array;

    ArrayType& getUnderlyingType()             { return m_array; }
    const ArrayType& getUnderlyingType() const { return m_array; }
    friend struct StdFunctionsMixin<ConstexprSizeBuffer>;

public:
    static constexpr bool k_needSizeInConstructor = false;

    // implicit constructors are just fine
};

template <typename Type>
class VectorBuffer
    : public StdFunctionsMixin<VectorBuffer<Type>>
{
    using Container = std::vector<Type>;
    Container m_container;

    Container& getUnderlyingType()             { return m_container; }
    const Container& getUnderlyingType() const { return m_container; }
    friend struct StdFunctionsMixin<VectorBuffer>;

public:
    static constexpr bool k_needSizeInConstructor = true;

    VectorBuffer(size_t size) : m_container(size + 1) {}
    VectorBuffer() = default;

    // implicit constructors are just fine
};

/**
 * Constructs a ring buffer in the 'Buffer' container adapter. 
 * It's important that the Buffer will reserve an additional sentinel element
 */
template <typename T, typename Buffer = VectorBuffer<T>>
class CircularBuffer
{
    /*
     * Types and iterators
    */
    static_assert(std::is_same_v<T, std::decay_t<T>>, "cv-qualified types aren't supported");

    using Pointer      = T*;
    using ConstPointer = const T*;

    static constexpr bool k_needSizeInConstructor = Buffer::k_needSizeInConstructor;

    template <typename PointerType>
    class IteratorImpl;

    template <typename OtherBuffer, typename BufferIteratorType>
    static ptrdiff_t getIndex(const OtherBuffer& otherBufer, const BufferIteratorType foreignIterator)
    {
        return foreignIterator - (& *std::begin(otherBufer));
    }

    struct PrivateDummy {};

public:

    using const_iterator = IteratorImpl<ConstPointer>;
    using iterator       = IteratorImpl<Pointer>;

    template <typename SizeType> 
    requires(k_needSizeInConstructor && std::is_integral_v<SizeType>)
    explicit CircularBuffer(SizeType capacity)
        : m_buffer(capacity)
        , m_head  (bufferBegin())
        , m_tail  (bufferBegin())
    {
    }

    template <typename Dummy = PrivateDummy>
    requires(!k_needSizeInConstructor && std::is_same_v<PrivateDummy, Dummy>)
    CircularBuffer(Dummy = PrivateDummy{})
        : m_buffer()
        , m_head(bufferBegin())
        , m_tail(bufferBegin())
    {
    }

    CircularBuffer(CircularBuffer&& temporary)
    {
        // may not swap pointers, adjust head/tail using displacements
        Displacements mine = *this;
        Displacements their = temporary;
        
        std::swap(this->m_buffer, temporary.m_buffer);    
        mine.applyTo(temporary);
        their.applyTo(*this);
    }

    CircularBuffer(const CircularBuffer& other)
        : m_buffer(other.m_buffer)
        , m_head  (bufferBegin() + getIndex(other.m_buffer, other.m_head))
        , m_tail  (bufferBegin() + getIndex(other.m_buffer, other.m_tail))
    {
    }

    CircularBuffer& operator=(CircularBuffer&& temporary)
    {
        if (this == &temporary)
            return *this;

        // may swap elements instead of pointers, thus we need to adjust head/tail using displacements
        Displacements mine  = *this;
        Displacements their = temporary;
        
        std::swap(this->m_buffer, temporary.m_buffer);    
        mine.applyTo(temporary);
        their.applyTo(*this);
        return *this;
    }

    CircularBuffer& operator=(const CircularBuffer& copy)
    {
        if (this == &copy)
            return *this;

        if constexpr (std::is_nothrow_copy_assignable_v<Buffer>)
        {
            m_buffer = copy.m_buffer;
            m_head = bufferBegin() + getIndex(copy.m_buffer, copy.m_head);
            m_tail = bufferBegin() + getIndex(copy.m_buffer, copy.m_tail);
        }
        else
        {
            *this = CircularBuffer(copy);   // temporary keeps me safe if constructor throws
        }

        return *this; 
    }

    size_t size() const 
    { 
        return m_tail >= m_head 
            ? (m_tail - m_head)                                    // tail is after the head:     {none, none, none, head, 2nd,  3rd, tail }  size=3
            : (bufferEnd() - m_head) + (m_tail - bufferBegin());   // tail is rolled before head: {3rd,  4th,  tail, none, none, head, 2nd }  size=4
    }

    bool empty() const                { return m_tail == m_head; }

    iterator       begin()            { return iterator      (bufferBegin(), bufferEnd(), m_head); }
    const_iterator begin()  const     { return const_iterator(bufferBegin(), bufferEnd(), m_head); }  // non-const buffer is needed for generic non-const iterator
    iterator       end()              { return iterator      (bufferBegin(), bufferEnd(), m_tail); }
    const_iterator end()    const     { return const_iterator(bufferBegin(), bufferEnd(), m_tail); }  // non-const buffer is needed for generic non-const iterator
    const_iterator cbegin() const     { return begin(); }
    const_iterator cend()   const     { return end(); }
    T&             back()             { return *std::prev(end()); } // don't prev(m_tail) because m_tail won't wrap around buffer edge
    const T&       back()   const     { return *std::prev(end()); } // don't prev(m_tail) because m_tail won't wrap around buffer edge
    T&             front()            { return *begin(); }
    const T&       front()   const    { return *begin(); }

    const_iterator findNthRecent(size_t requestedCount) const
    { 
        requestedCount = std::min(requestedCount, size());
        ConstPointer start = m_tail - requestedCount;

        // wrap if exceeded before begin of the buffer
        ptrdiff_t startIndex = start - bufferBegin();
        if (startIndex < 0)
            start = bufferEnd() + startIndex;

        return const_iterator(bufferBegin(), bufferEnd(), start);
    }

    auto mostRecent(size_t count) const &
    {
        return std::ranges::subrange(findNthRecent(count), cend());
    }

    void mostRecent(size_t count) && = delete;                  // can't return a subrange of a temporary

    template <typename Convertible>
    T& pushBack(Convertible&& rvalue)
    { 
        *m_tail = std::forward<Convertible>(rvalue);
        T& insertedRef = *m_tail;

        if (++m_tail == bufferEnd())
            m_tail = bufferBegin();    // wrap around the buffer end

        if (m_tail == m_head)
            shiftFront();           // popFront in case of overflow

        return insertedRef;
    }

    void popFront()
    {
        if (!empty() && ++m_head == bufferEnd())
            m_head = bufferBegin();
    }

private:

    Buffer  m_buffer;
    Pointer m_head = nullptr;      // first element
    Pointer m_tail = nullptr;      // past the last element, technically, may be before first because this is ring buffer

    ConstPointer bufferBegin() const { return & *std::begin(m_buffer); }
    Pointer      bufferBegin()       { return & *std::begin(m_buffer); }
    ConstPointer bufferEnd() const   { return bufferBegin() + std::size(m_buffer); }
    Pointer      bufferEnd()         { return bufferBegin() + std::size(m_buffer); }

    void shiftFront()
    {
        if (++m_head == bufferEnd())
            m_head = bufferBegin();
    }

    struct Displacements
    {
        ptrdiff_t m_head = 0;
        ptrdiff_t m_tail = 0;

        Displacements(const CircularBuffer &b)
            : m_head(b.m_head - b.bufferBegin()), m_tail(b.m_tail - b.bufferBegin())
        {
        }

        void applyTo(CircularBuffer &b)
        {
            b.m_head = b.bufferBegin() + m_head;
            b.m_tail = b.bufferBegin() + m_tail;
        }
    };
};

template <typename T, typename ContainerType>
template <typename PointerType>
class CircularBuffer<T, ContainerType>::IteratorImpl
{
    PointerType m_bufferBegin = {};
    PointerType m_bufferEnd   = {};
    PointerType m_current     = {};

public:
    using iterator_category = std::bidirectional_iterator_tag;
    using difference_type   = decltype(std::declval<PointerType>() - std::declval<PointerType>());
    using value_type        = std::remove_pointer_t<PointerType>;
    using pointer           = PointerType;
    using reference         = decltype(*std::declval<PointerType>());

    IteratorImpl()                    = default;   // iterator must be default-initializable for ranges compatibility
    IteratorImpl(const IteratorImpl&) = default;
    IteratorImpl(IteratorImpl&&)      = default;
    IteratorImpl& operator=(const IteratorImpl&) = default;
    IteratorImpl& operator=(IteratorImpl&&)      = default;

    IteratorImpl(PointerType bufferBegin, PointerType bufferEnd, PointerType bufferPos)
        : m_bufferBegin(bufferBegin)
        , m_bufferEnd(bufferEnd)
        , m_current(bufferPos)
    {}

    IteratorImpl& operator++()
    {
        assert(*this != IteratorImpl{});
        if(++m_current == m_bufferEnd)
            m_current = m_bufferBegin;
        return *this;
    }

    IteratorImpl operator++(int)
    {
        assert(*this != IteratorImpl{});
        IteratorImpl ret = *this;
        ++(*this);
        return ret;
    }

    IteratorImpl& operator--()
    {
        assert(*this != IteratorImpl{});
        if (--m_current < m_bufferBegin)
            m_current = std::prev(m_bufferEnd);
        return *this;
    }

    IteratorImpl operator--(int)
    {
        assert(*this != IteratorImpl{});
        IteratorImpl ret = *this;
        --(*this);
        return ret;
    }

    friend bool operator==(const IteratorImpl& left, const IteratorImpl& right) { return left.m_current == right.m_current; }
    friend bool operator!=(const IteratorImpl& left, const IteratorImpl& right) { return left.m_current != right.m_current; }

    reference operator*()  const
    {
        assert(*this != IteratorImpl{});
        return *m_current;
    }

    pointer   operator->() const
    {
        assert(*this != IteratorImpl{});
        return m_current;
    }

    operator IteratorImpl<ConstPointer>() const { return IteratorImpl<ConstPointer>(m_bufferBegin, m_bufferEnd, m_current); }
};

