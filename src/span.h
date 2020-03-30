// Copyright (c) 2018 The Bitcoin Core developers
// Copyright (c) 2020 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <type_traits>

/** A Span is an object that can refer to a contiguous sequence of objects.
 *
 * It implements a subset of C++20's std::span.
 */
template <typename C> class Span {
    C *m_data{};
    std::size_t m_size{};

public:
    constexpr Span() noexcept = default;

    /** Construct a span from a begin pointer and a size.
     *
     * This implements a subset of the iterator-based std::span constructor in C++20,
     * which is hard to implement without std::address_of.
     */
    template <typename T, typename std::enable_if_t<std::is_convertible_v<T (*)[], C (*)[]>, int> = 0>
    constexpr Span(T *begin, std::size_t size) noexcept : m_data(begin), m_size(size) {}

    /** Construct a span from a begin and end pointer.
     *
     * This implements a subset of the iterator-based std::span constructor in C++20,
     * which is hard to implement without std::address_of.
     */
    template <typename T, typename std::enable_if_t<std::is_convertible_v<T (*)[], C (*)[]>, int> = 0>
    constexpr Span(T *begin, T *end) noexcept : m_data(begin), m_size(end - begin) {}

    /** Implicit conversion of spans between compatible types.
     *
     *  Specifically, if a pointer to an array of type O can be implicitly converted to a pointer to an array of type
     *  C, then permit implicit conversion of Span<O> to Span<C>. This matches the behavior of the corresponding
     *  C++20 std::span constructor.
     *
     *  For example this means that a Span<T> can be converted into a Span<const T>.
     */
    template <typename O, typename std::enable_if_t<std::is_convertible_v<O (*)[], C (*)[]>, int> = 0>
    constexpr Span(const Span<O>& other) noexcept : m_data(other.m_data), m_size(other.m_size) {}

    /** Default copy constructor. */
    constexpr Span(const Span&) noexcept = default;

    /** Construct a Span from an array. This matches the corresponding C++20 std::span constructor. */
    template <std::size_t N>
    constexpr Span(C (&a)[N]) noexcept : m_data(a), m_size(N) {}

    /** Construct a Span for objects with .data() and .size() (std::string, std::array, std::vector, ...).
     *
     * This implements a subset of the functionality provided by the C++20 std::span range-based constructor.
     *
     * To prevent surprises, only Spans for constant value types are supported when passing in temporaries.
     * Note that this restriction does not exist when converting arrays or other Spans (see above).
     */
    template <typename V,
              typename std::enable_if_t<
                  (std::is_const_v<C> || std::is_lvalue_reference_v<V>) && std::is_convertible_v<
                      typename std::remove_pointer_t<decltype(std::declval<V &>().data())> (*)[], C (*)[]> &&
                      std::is_convertible_v<decltype(std::declval<V &>().size()), std::size_t>,
                  int> = 0>
    constexpr Span(V &&v) noexcept : m_data(v.data()), m_size(v.size()) {}

    /** Default assignment operator. */
    Span& operator=(const Span& other) noexcept = default;

    constexpr C *data() const noexcept { return m_data; }
    constexpr C *begin() const noexcept { return m_data; }
    constexpr C *end() const noexcept { return m_data + m_size; }
    constexpr std::size_t size() const noexcept { return m_size; }
    constexpr bool empty() const noexcept { return size() == 0; }
    constexpr C &operator[](std::size_t pos) const noexcept { return m_data[pos]; }
    constexpr C &front() const noexcept { return *begin(); }
    constexpr C &back() const noexcept { return *(end()-1); }

    constexpr Span<C> subspan(std::size_t offset) const noexcept {
        return offset <= m_size? Span<C>(m_data + offset, m_size - offset) : Span<C>(end(), std::size_t{0});
    }
    constexpr Span<C> subspan(std::size_t offset, std::size_t count) const noexcept {
        return offset + count <= m_size ? Span<C>(m_data + offset, count) : Span<C>(end(), std::size_t{0});
    }
    constexpr Span<C> first(std::size_t count) const noexcept {
        return count <= m_size ? Span<C>(m_data, count) : Span<C>(begin(), std::size_t{0});
    }
    constexpr Span<C> last(std::size_t count) const noexcept {
        return count <= m_size ? Span<C>(m_data + m_size - count, count) : Span<C>(end(), std::size_t{0});
    }

    /** Pop the last element off, and return a reference to that element.
        Span must not be empty(); span will decrease in size by 1, having its end() moved back by 1. */
    constexpr C & pop_back() noexcept {
        assert(!empty());
        return m_data[--m_size];
    }

    /** Pop the last element off, and return a reference to that element.
        Span must not be empty(); span will decrease in size by 1, having its begin() moved up by 1. */
    constexpr C & pop_front() noexcept {
        assert(!empty());
        --m_size;
        return *m_data++;
    }

    friend constexpr bool operator==(const Span &a, const Span &b) noexcept {
        return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin());
    }
    friend constexpr bool operator!=(const Span &a, const Span &b) noexcept {
        return !(a == b);
    }
    friend constexpr bool operator<(const Span &a, const Span &b) noexcept {
        return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end());
    }
    friend constexpr bool operator<=(const Span &a, const Span &b) noexcept {
        return !(b < a);
    }
    friend constexpr bool operator>(const Span &a, const Span &b) noexcept {
        return b < a;
    }
    friend constexpr bool operator>=(const Span &a, const Span &b) noexcept {
        return !(a < b);
    }

    /** Ensures the convertible-to constructor works */
    template <typename O> friend class Span;
};

// MakeSpan helps constructing a Span of the right type automatically.
/** MakeSpan for arrays: */
template <typename A, std::size_t N>
constexpr Span<A> MakeSpan(A (&a)[N]) {
    return Span<A>(a, N);
}
/** MakeSpan for temporaries / rvalue references, only supporting const output. */
template <typename V>
constexpr auto MakeSpan(V &&v) ->
    typename std::enable_if_t<!std::is_lvalue_reference_v<V>,
                              Span<const typename std::remove_pointer_t<decltype(v.data())>>> {
    return std::forward<V>(v);
}
/** MakeSpan for (lvalue) references, supporting mutable output. */
template <typename V>
constexpr auto MakeSpan(V &v) -> Span<typename std::remove_pointer_t<decltype(v.data())>> {
    return v;
}
