#pragma once

#include <iterator>
#include <vector>

#include <cstddef>      // ptrdiff_t
#include <type_traits>


namespace detail
{
    // construct std::vector-like containers
    template <typename T>
    static T constructBuffer(size_t capacity)
    {
        return T(capacity);
    }

    // generic adapter is not implemented, use specialization instead
    template <class Container> class BufferAdapter;

    // adds begin/end/size functions to its 'Derived' subclass 
    // assumes that there is getContainer() method returns a reference to the underlying class that supports std::begin/end/size, 
    // e.g. 'std::vector<T>& getContainer();'
    struct StdFunctionsMixin
    {
        auto begin (this auto&& self)  { return std::begin(self.getContainer()); }
        auto cbegin(this auto&& self)  { return std::cbegin(self.getContainer()); }

        auto end (this auto&& self)    { return std::end (self.getContainer()); }
        auto cend(this auto&& self)    { return std::cend(self.getContainer()); }

        auto size(this auto&& self)    { return std::size(self.getContainer()); }
    };

    // std::vector specialization
    template <typename... Args>
    class BufferAdapter< std::vector<Args...> >
        : public StdFunctionsMixin
    {
        using ThisType = BufferAdapter<std::vector<Args...>>;

        using VectorType = std::vector<Args...>;
        VectorType m_vector;

        BufferAdapter(VectorType&& container) : m_vector(std::move(container)) { }

        auto&& getContainer(this auto&& self)       { return self.m_vector; } 
        friend struct StdFunctionsMixin;

    public:
        static constexpr bool hasSizedMake = true;
        static ThisType make(size_t size) { return ThisType(VectorType(size)); }
    };

    // std::array specialization
    template <typename Type, size_t Size>
    class BufferAdapter< std::array<Type, Size> >
        : public StdFunctionsMixin
    {
        using ThisType  = BufferAdapter< std::array<Type, Size> >;
        using ArrayType = std::array<Type, Size + 1/*sentinel element*/>;
        ArrayType m_array;

        auto&& getContainer(this auto&& self)       { return self.m_array; } 
        friend struct StdFunctionsMixin;

    public:
        static constexpr bool hasSizedMake = false;
        static ThisType make() { return ThisType(); }
    };

}

template <typename T, typename ContainerType = std::vector<T>>
class CircularBuffer
{
    /*
     * Types and iterators
    */
    static_assert(std::is_same_v<T, std::decay_t<T>>, "cv-qualified types aren't supported");

    using Pointer      = T*;
    using ConstPointer = const T*;

    template <typename PointerType>
    class IteratorImpl;

    template <typename OtherBuffer, typename BufferIteratorType>
    static ptrdiff_t getIndex(const OtherBuffer& otherBufer, const BufferIteratorType foreignIterator)
    {
        return foreignIterator - (& *std::begin(otherBufer));
    }

public:

    using const_iterator = IteratorImpl<ConstPointer>;
    using iterator       = IteratorImpl<Pointer>;

    template <typename SizeType, typename Dummy = std::invoke_result<decltype(&detail::BufferAdapter<ContainerType>::make), SizeType> >
    explicit CircularBuffer(SizeType capacity)
        : m_buffer(detail::BufferAdapter<ContainerType>::make(capacity + 1/*sentinel for pushBack*/))
        , m_head  (bufferBegin())
        , m_tail  (bufferBegin())
    {
    }

    template <typename Dummy = std::invoke_result<decltype(&detail::BufferAdapter<ContainerType>::make)> >
    CircularBuffer(Dummy* = nullptr)
        : m_buffer(detail::BufferAdapter<ContainerType>::make())
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
        // may not swap pointers, adjust head/tail using displacements
        Displacements mine  = *this;
        Displacements their = temporary;
        
        std::swap(this->m_buffer, temporary.m_buffer);    
        mine.applyTo(temporary);
        their.applyTo(*this);
        return *this;
    }

    CircularBuffer& operator=(const CircularBuffer& copy)
    {
        CircularBuffer newBuffer = copy;   // keep me safe if it throws
        return *this = std::move(newBuffer);
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

    auto&& back(this auto&& self)  { return *std::prev( self.end() ); }  
    auto&& front(this auto&& self) { return *self.begin(); }  

    const_iterator mostRecent(size_t requestedCount) const
    { 
        requestedCount = std::min(requestedCount, size());
        ConstPointer start = m_tail - requestedCount;

        // wrap if exceeded before begin of the buffer
        ptrdiff_t startIndex = start - bufferBegin();
        if (startIndex < 0)
            start = bufferEnd() + startIndex;

        return const_iterator(bufferBegin(), bufferEnd(), start);
    }

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

    detail::BufferAdapter<ContainerType> m_buffer;
    Pointer m_head;      // first element
    Pointer m_tail;      // past the last element, technically, may be before first because this is ring buffer

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
    PointerType const m_bufferBegin;
    PointerType const m_bufferEnd;
    PointerType       m_current;

public:
    using iterator_category = std::bidirectional_iterator_tag;
    using difference_type = decltype(PointerType(0) - PointerType(0));
    using value_type = std::remove_pointer_t<PointerType>;
    using pointer = PointerType;
    using reference = decltype(*PointerType(0));

    IteratorImpl(PointerType bufferBegin, PointerType bufferEnd, PointerType bufferPos)
        : m_bufferBegin(bufferBegin)
        , m_bufferEnd(bufferEnd)
        , m_current(bufferPos)
    {}

    IteratorImpl& operator++()
    {
        if(++m_current == m_bufferEnd)
            m_current = m_bufferBegin;
        return *this;
    }

    IteratorImpl operator++(int)
    {
        IteratorImpl ret = *this;
        ++(*this);
        return ret;
    }

    IteratorImpl& operator--()
    {
        if(--m_current < m_bufferBegin)
            m_current = std::prev(m_bufferEnd);
        return *this;
    }

    IteratorImpl operator--(int)
    {
        IteratorImpl ret = *this;
        --(*this);
        return ret;
    }

    friend bool operator==(const IteratorImpl& left, const IteratorImpl& right) { return left.m_current == right.m_current; }
    friend bool operator!=(const IteratorImpl& left, const IteratorImpl& right) { return left.m_current != right.m_current; }

    reference operator*() const { return *m_current; }
    pointer   operator->() const { return m_current; }

    operator IteratorImpl<ConstPointer>() const { return IteratorImpl<ConstPointer>(m_bufferBegin, m_bufferEnd, m_current); }
};

