// Copyright 2014 BitPay Inc.
// Copyright 2015 Bitcoin Core Developers
// Copyright (c) 2020-2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef __UNIVALUE_H__
#define __UNIVALUE_H__

#include <cassert>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

class UniValue {
public:

    /**
     * Value types available in JSON (and thus in UniValue).
     * Every type sets a different bit, so bitmasks can be used.
     * Can be used with is().
     *
     * Numeric values differ from the upstream UniValue API.
     * VFALSE and VTRUE are Radiant Node extensions of the UniValue API (replacing VBOOL).
     */
    enum VType {
        VNULL  = 1 << 0,
        VFALSE = 1 << 1,
        VTRUE  = 1 << 2,
        VOBJ   = 1 << 3,
        VARR   = 1 << 4,
        VNUM   = 1 << 5,
        VSTR   = 1 << 6,
    };

    /**
     * Type bitmask shorthand for VFALSE|VTRUE.
     * Can be used with is().
     *
     * This is a Radiant Node extension of the UniValue API.
     */
    constexpr static auto MBOOL = VFALSE | VTRUE;

    class Object {

    public:
        using mapped_type = UniValue;
        using key_type = std::string;
        using value_type = std::pair<key_type, mapped_type>;

    private:
        using Vector = std::vector<value_type>;
        Vector vector;

    public:
        using size_type = Vector::size_type;
        using iterator = Vector::iterator;
        using const_iterator = Vector::const_iterator;
        using reverse_iterator = Vector::reverse_iterator;
        using const_reverse_iterator = Vector::const_reverse_iterator;

        Object() noexcept = default;
        explicit Object(const Object&) = default;
        Object(Object&&) noexcept = default;
        Object& operator=(const Object&) = default;
        Object& operator=(Object&&) = default;

        /**
         * Returns an iterator to the first key-value pair of the object.
         *
         * The returned iterator follows the iterator invalidation rules of the underlying vector.
         *
         * Complexity: constant.
         */
        [[nodiscard]]
        const_iterator begin() const noexcept { return vector.begin(); }
        [[nodiscard]]
        iterator begin() noexcept { return vector.begin(); }

        /**
         * Returns an iterator to the past-the-last key-value pair of the object.
         *
         * The returned iterator follows the iterator invalidation rules of the underlying vector.
         *
         * Complexity: constant.
         */
        [[nodiscard]]
        const_iterator end() const noexcept { return vector.end(); }
        [[nodiscard]]
        iterator end() noexcept { return vector.end(); }

        /**
         * Returns an iterator to the first key-value pair of the reversed object.
         *
         * The returned iterator follows the iterator invalidation rules of the underlying vector.
         *
         * Complexity: constant.
         */
        [[nodiscard]]
        const_reverse_iterator rbegin() const noexcept { return vector.rbegin(); }
        [[nodiscard]]
        reverse_iterator rbegin() noexcept { return vector.rbegin(); }

        /**
         * Returns an iterator to the past-the-last key-value pair of the reversed object.
         *
         * The returned iterator follows the iterator invalidation rules of the underlying vector.
         *
         * Complexity: constant.
         */
        [[nodiscard]]
        const_reverse_iterator rend() const noexcept { return vector.rend(); }
        [[nodiscard]]
        reverse_iterator rend() noexcept { return vector.rend(); }

        /**
         * Removes all key-value pairs from the object.
         *
         * Complexity: linear in number of elements.
         */
        void clear() noexcept { vector.clear(); }

        /**
         * Returns whether the object is empty.
         *
         * Complexity: constant.
         */
        [[nodiscard]]
        bool empty() const noexcept { return vector.empty(); }

        /**
         * Returns the size of the object.
         *
         * Complexity: constant.
         */
        [[nodiscard]]
        size_type size() const noexcept { return vector.size(); }

        /**
         * Increases the capacity of the underlying vector to at least new_cap.
         *
         * Complexity: at most linear in number of elements.
         */
        void reserve(size_type new_cap) { vector.reserve(new_cap); }

        /**
         * Returns a reference to the first value associated with the key,
         * or NullUniValue if the key does not exist.
         *
         * The returned reference follows the iterator invalidation rules of the underlying vector.
         *
         * Complexity: linear in number of elements.
         *
         * If you want to distinguish between null values and missing keys, please use locate() instead.
         */
        [[nodiscard]]
        const UniValue& operator[](std::string_view key) const noexcept;

        /**
         * Returns a reference to the value at the numeric index (regardless of key),
         * or NullUniValue if index >= object size.
         *
         * The returned reference follows the iterator invalidation rules of the underlying vector.
         *
         * Complexity: constant.
         *
         * To access the first or last value, consider using front() or back() instead.
         * If you want an exception thrown on missing indices, please use at() instead.
         */
        [[nodiscard]]
        const UniValue& operator[](size_type index) const noexcept;

        /**
         * Returns a pointer to the first value associated with the key,
         * or nullptr if the key does not exist.
         *
         * The returned pointer follows the iterator invalidation rules of the underlying vector.
         *
         * Complexity: linear in the number of elements.
         *
         * If you want to treat missing keys as null values, please use the [] operator instead.
         * If you want an exception thrown on missing keys, please use at() instead.
         */
        [[nodiscard]]
        const UniValue* locate(std::string_view key) const noexcept;
        [[nodiscard]]
        UniValue* locate(std::string_view key) noexcept;

        /**
         * Returns a reference to the first value associated with the key,
         * or throws std::out_of_range if the key does not exist.
         *
         * The returned reference follows the iterator invalidation rules of the underlying vector.
         *
         * Complexity: linear in number of elements.
         *
         * If you don't want an exception thrown, please use locate() or the [] operator instead.
         */
        const UniValue& at(std::string_view key) const;
        UniValue& at(std::string_view key);

        /**
         * Returns a reference to the value at the numeric index (regardless of key),
         * or throws std::out_of_range if index >= object size.
         *
         * The returned reference follows the iterator invalidation rules of the underlying vector.
         *
         * Complexity: constant.
         *
         * If you don't want an exception thrown, please use the [] operator instead.
         */
        const UniValue& at(size_type index) const;
        UniValue& at(size_type index);

        /**
         * Returns a reference to the first value (regardless of key),
         * or NullUniValue if the object is empty.
         *
         * The returned reference follows the iterator invalidation rules of the underlying vector.
         *
         * Complexity: constant.
         */
        [[nodiscard]]
        const UniValue& front() const noexcept;

        /**
         * Returns a reference to the last value (regardless of key),
         * or NullUniValue if the object is empty.
         *
         * The returned reference follows the iterator invalidation rules of the underlying vector.
         *
         * Complexity: constant.
         */
        [[nodiscard]]
        const UniValue& back() const noexcept;

        /**
         * Pushes the key-value pair onto the end of the object.
         *
         * Be aware that this method appends the new key-value pair regardless of whether the key already exists.
         * If you want to avoid duplicate keys, use locate() and check its result before calling push_back().
         * You can use the return value of locate() to update the existing value associated with the key, if any.
         *
         * Complexity: amortized constant (or constant if properly reserve()d).
         */
        void push_back(const value_type& entry) { vector.push_back(entry); }
        void push_back(value_type&& entry) { vector.push_back(std::move(entry)); }

        /**
         * Constructs a key-value pair in-place at the end of the object.
         *
         * Be aware that this method appends the new key-value pair regardless of whether the key already exists.
         * If you want to avoid duplicate keys, use locate() and check its result before calling emplace_back().
         * You can use the return value of locate() to update the existing value associated with the key, if any.
         *
         * Complexity: amortized constant (or constant if properly reserve()d).
         */
        template<class... Args>
        void emplace_back(Args&&... args) { vector.emplace_back(std::forward<Args>(args)...); }

        /**
         * Removes the key-value pairs in the range [first, last).
         * Returns the iterator following the last removed key-value pair.
         *
         * Complexity: linear in the number of elements removed and linear in the number of elements after those.
         */
        iterator erase(const_iterator first, const_iterator last) { return vector.erase(first, last); }

        /**
         * Returns whether the objects contain equal data.
         * Two objects are not considered equal if elements are ordered differently.
         *
         * Complexity: linear in the amount of data to compare.
         */
        [[nodiscard]]
        bool operator==(const Object& other) const noexcept { return vector == other.vector; }

        /**
         * Returns whether the objects contain unequal data.
         * Two objects are not considered equal if elements are ordered differently.
         *
         * Complexity: linear in the amount of data to compare.
         */
        [[nodiscard]]
        bool operator!=(const Object& other) const noexcept { return !(*this == other); }
    };

    class Array {

    public:
        using value_type = UniValue;

    private:
        using Vector = std::vector<value_type>;
        Vector vector;

    public:
        using size_type = Vector::size_type;
        using iterator = Vector::iterator;
        using const_iterator = Vector::const_iterator;
        using reverse_iterator = Vector::reverse_iterator;
        using const_reverse_iterator = Vector::const_reverse_iterator;

        Array() noexcept = default;
        explicit Array(const Array&) = default;
        Array(Array&&) noexcept = default;
        Array& operator=(const Array&) = default;
        Array& operator=(Array&&) = default;

        /**
         * Returns an iterator to the first value of the array.
         *
         * The returned iterator follows the iterator invalidation rules of the underlying vector.
         *
         * Complexity: constant.
         */
        [[nodiscard]]
        const_iterator begin() const noexcept { return vector.begin(); }
        [[nodiscard]]
        iterator begin() noexcept { return vector.begin(); }

        /**
         * Returns an iterator to the past-the-last value of the array.
         *
         * The returned iterator follows the iterator invalidation rules of the underlying vector.
         *
         * Complexity: constant.
         */
        [[nodiscard]]
        const_iterator end() const noexcept { return vector.end(); }
        [[nodiscard]]
        iterator end() noexcept { return vector.end(); }

        /**
         * Returns an iterator to the first value of the reversed array.
         *
         * The returned iterator follows the iterator invalidation rules of the underlying vector.
         *
         * Complexity: constant.
         */
        [[nodiscard]]
        const_reverse_iterator rbegin() const noexcept { return vector.rbegin(); }
        [[nodiscard]]
        reverse_iterator rbegin() noexcept { return vector.rbegin(); }

        /**
         * Returns an iterator to the past-the-last value of the reversed array.
         *
         * The returned iterator follows the iterator invalidation rules of the underlying vector.
         *
         * Complexity: constant.
         */
        [[nodiscard]]
        const_reverse_iterator rend() const noexcept { return vector.rend(); }
        [[nodiscard]]
        reverse_iterator rend() noexcept { return vector.rend(); }

        /**
         * Removes all values from the array.
         *
         * Complexity: linear in number of elements.
         */
        void clear() noexcept { vector.clear(); }

        /**
         * Returns whether the array is empty.
         *
         * Complexity: constant.
         */
        [[nodiscard]]
        bool empty() const noexcept { return vector.empty(); }

        /**
         * Returns the size of the array.
         *
         * Complexity: constant.
         */
        [[nodiscard]]
        size_type size() const noexcept { return vector.size(); }

        /**
         * Increases the capacity of the underlying vector to at least new_cap.
         *
         * Complexity: at most linear in number of elements.
         */
        void reserve(size_type new_cap) { vector.reserve(new_cap); }

        /**
         * Returns a reference to the value at the index,
         * or NullUniValue if index >= array size.
         *
         * The returned reference follows the iterator invalidation rules of the underlying vector.
         *
         * Complexity: constant.
         *
         * To access the first or last value, consider using front() or back() instead.
         * If you want an exception thrown on missing indices, please use at() instead.
         */
        [[nodiscard]]
        const UniValue& operator[](size_type index) const noexcept;

        /**
         * Returns a reference to the value at the index,
         * or throws std::out_of_range if index >= array size.
         *
         * The returned reference follows the iterator invalidation rules of the underlying vector.
         *
         * Complexity: constant.
         *
         * If you don't want an exception thrown, please use the [] operator instead.
         */
        const UniValue& at(size_type index) const;
        UniValue& at(size_type index);

        /**
         * Returns a reference to the first value,
         * or NullUniValue if the array is empty.
         *
         * The returned reference follows the iterator invalidation rules of the underlying vector.
         *
         * Complexity: constant.
         */
        [[nodiscard]]
        const UniValue& front() const noexcept;

        /**
         * Returns a reference to the last value,
         * or NullUniValue if the array is empty.
         *
         * The returned reference follows the iterator invalidation rules of the underlying vector.
         *
         * Complexity: constant.
         */
        [[nodiscard]]
        const UniValue& back() const noexcept;

        /**
         * Pushes the value onto the end of the array.
         *
         * Complexity: amortized constant (or constant if properly reserve()d).
         */
        void push_back(const value_type& entry) { vector.push_back(entry); }
        void push_back(value_type&& entry) { vector.push_back(std::move(entry)); }

        /**
         * Constructs a value in-place at the end of the array.
         *
         * Complexity: amortized constant (or constant if properly reserve()d).
         */
        template<class... Args>
        void emplace_back(Args&&... args) { vector.emplace_back(std::forward<Args>(args)...); }

        /**
         * Removes the values in the range [first, last).
         * Returns the iterator following the last removed value.
         *
         * Complexity: linear in the number of elements removed and linear in the number of elements after those.
         */
        iterator erase(const_iterator first, const_iterator last) { return vector.erase(first, last); }

        /**
         * Returns whether the arrays contain equal data.
         *
         * Complexity: linear in the amount of data to compare.
         */
        [[nodiscard]]
        bool operator==(const Array& other) const noexcept { return vector == other.vector; }

        /**
         * Returns whether the arrays contain unequal data.
         *
         * Complexity: linear in the amount of data to compare.
         */
        [[nodiscard]]
        bool operator!=(const Array& other) const noexcept { return !(*this == other); }
    };

    using size_type = Object::size_type;
    static_assert(std::is_same<size_type, Array::size_type>::value,
                  "UniValue::size_type should be equal to both UniValue::Object::size_type and UniValue::Array::size_type.");

    explicit UniValue(VType initialType = VNULL) noexcept : typ(initialType) {}
    UniValue(VType initialType, std::string_view initialStr) : typ(initialType), val(initialStr) {}
    UniValue(VType initialType, std::string&& initialStr) noexcept : typ(initialType), val(std::move(initialStr)) {}
    UniValue(VType initialType, const char* initialStr) : typ(initialType), val(initialStr) {}
    explicit UniValue(const UniValue&) = default;
    UniValue(UniValue&&) noexcept = default;
    UniValue& operator=(const UniValue&) = default;
    UniValue& operator=(UniValue&&) noexcept = default;

    UniValue(bool val_) noexcept : typ(val_ ? VTRUE : VFALSE) {}
    explicit UniValue(const Object& object) : typ(VOBJ), entries(object) {}
    UniValue(Object&& object) noexcept : typ(VOBJ), entries(std::move(object)) {}
    explicit UniValue(const Array& array) : typ(VARR), values(array) {}
    UniValue(Array&& array) noexcept : typ(VARR), values(std::move(array)) {}
    UniValue(short val_) { *this = val_; }
    UniValue(int val_) { *this = val_; }
    UniValue(long val_) { *this = val_; }
    UniValue(long long val_) { *this = val_; }
    UniValue(unsigned short val_) { *this = val_; }
    UniValue(unsigned val_) { *this = val_; }
    UniValue(unsigned long val_) { *this = val_; }
    UniValue(unsigned long long val_) { *this = val_; }
    UniValue(double val_) { *this = val_; }
    UniValue(std::string_view val_) : typ(VSTR), val(val_) {}
    UniValue(std::string&& val_) noexcept : typ(VSTR), val(std::move(val_)) {}
    UniValue(const char* val_) : typ(VSTR), val(val_) {}

    void setNull() noexcept;
    void operator=(bool val) noexcept;
    Object& setObject() noexcept;
    Object& operator=(const Object& object);
    Object& operator=(Object&& object) noexcept;
    Array& setArray() noexcept;
    Array& operator=(const Array& array);
    Array& operator=(Array&& array) noexcept;
    void setNumStr(const char* val); // TODO: refactor to assign null on failure
    void operator=(short val);
    void operator=(int val);
    void operator=(long val);
    void operator=(long long val);
    void operator=(unsigned short val);
    void operator=(unsigned val);
    void operator=(unsigned long val);
    void operator=(unsigned long long val);
    void operator=(double val);
    std::string& operator=(std::string_view val);
    std::string& operator=(std::string&& val) noexcept;
    std::string& operator=(const char* val_) { return *this = std::string_view(val_); }

    [[nodiscard]]
    constexpr VType getType() const noexcept { return typ; }
    [[nodiscard]]
    constexpr const std::string& getValStr() const noexcept { return val; }

    /**
     * VOBJ/VARR: Returns whether the object/array is empty.
     * Other types: Returns true.
     *
     * Complexity: constant.
     *
     * Compatible with the upstream UniValue API.
     */
    [[nodiscard]]
    bool empty() const noexcept {
        switch (typ) {
        case VOBJ:
            return entries.empty();
        case VARR:
            return values.empty();
        default:
            return true;
        }
    }

    /**
     * VOBJ/VARR: Returns the size of the object/array.
     * Other types: Returns zero.
     *
     * Complexity: constant.
     *
     * Compatible with the upstream UniValue API.
     */
    [[nodiscard]]
    size_type size() const noexcept {
        switch (typ) {
        case VOBJ:
            return entries.size();
        case VARR:
            return values.size();
        default:
            return 0;
        }
    }

    [[nodiscard]]
    constexpr bool getBool() const noexcept { return isTrue(); }

    /**
     * VOBJ: Returns a reference to the first value associated with the key,
     *       or NullUniValue if the key does not exist.
     * Other types: Returns NullUniValue.
     *
     * The returned reference follows the iterator invalidation rules of the underlying vector.
     *
     * Complexity: linear in number of elements.
     *
     * Compatible with the upstream UniValue API.
     *
     * If you want to distinguish between null values and missing keys, please use locate() instead.
     * If you want an exception thrown on missing keys, please use at() instead.
     */
    [[nodiscard]]
    const UniValue& operator[](std::string_view key) const noexcept;

    /**
     * VOBJ: Returns a reference to the value at the numeric index (regardless of key),
     *       or NullUniValue if index >= object size.
     * VARR: Returns a reference to the element at the index,
     *       or NullUniValue if index >= array size.
     * Other types: Returns NullUniValue.
     *
     * The returned reference follows the iterator invalidation rules of the underlying vector.
     *
     * Complexity: constant.
     *
     * Compatible with the upstream UniValue API.
     *
     * To access the first or last value, consider using front() or back() instead.
     * If you want an exception thrown on missing indices, please use at() instead.
     */
    [[nodiscard]]
    const UniValue& operator[](size_type index) const noexcept;

    /**
     * Returns whether the UniValues are of the same type and contain equal data.
     * Two objects/arrays are not considered equal if elements are ordered differently.
     *
     * Complexity: linear in the amount of data to compare.
     *
     * This is a Radiant Node extension of the UniValue API.
     */
    [[nodiscard]]
    bool operator==(const UniValue& other) const noexcept;

    /**
     * Returns whether the UniValues are not of the same type or contain unequal data.
     * Two objects/arrays are not considered equal if elements are ordered differently.
     *
     * Complexity: linear in the amount of data to compare.
     *
     * This is a Radiant Node extension of the UniValue API.
     */
    [[nodiscard]]
    bool operator!=(const UniValue& other) const noexcept { return !(*this == other); }

    /**
     * VOBJ: Returns a reference to the first value (regardless of key),
     *       or NullUniValue if the object is empty.
     * VARR: Returns a reference to the first element,
     *       or NullUniValue if the array is empty.
     * Other types: Returns NullUniValue.
     *
     * The returned reference follows the iterator invalidation rules of the underlying vector.
     *
     * Complexity: constant.
     *
     * This is a Radiant Node extension of the UniValue API.
     */
    [[nodiscard]]
    const UniValue& front() const noexcept;

    /**
     * VOBJ: Returns a reference to the last value (regardless of key),
     *       or NullUniValue if the object is empty.
     * VARR: Returns a reference to the last element,
     *       or NullUniValue if the array is empty.
     * Other types: Returns NullUniValue.
     *
     * The returned reference follows the iterator invalidation rules of the underlying vector.
     *
     * Complexity: constant.
     *
     * This is a Radiant Node extension of the UniValue API.
     */
    [[nodiscard]]
    const UniValue& back() const noexcept;

    /**
     * VOBJ: Returns a pointer to the first value associated with the key,
     *       or nullptr if the key does not exist.
     * Other types: Returns nullptr.
     *
     * The returned pointer follows the iterator invalidation rules of the underlying vector.
     *
     * Complexity: linear in the number of elements.
     *
     * This is a Radiant Node extension of the UniValue API.
     *
     * If you want to treat missing keys as null values, please use the [] operator instead.
     * If you want an exception thrown on missing keys, please use at() instead.
     */
    [[nodiscard]]
    const UniValue* locate(std::string_view key) const noexcept;
    [[nodiscard]]
    UniValue* locate(std::string_view key) noexcept;

    /**
     * VOBJ: Returns a reference to the first value associated with the key,
     *       or throws std::out_of_range if the key does not exist.
     * Other types: Throws std::domain_error.
     *
     * The returned reference follows the iterator invalidation rules of the underlying vector.
     *
     * Complexity: linear in number of elements.
     *
     * This is a Radiant Node extension of the UniValue API.
     *
     * If you don't want an exception thrown, please use locate() or the [] operator instead.
     */
    const UniValue& at(std::string_view key) const;
    UniValue& at(std::string_view key);

    /**
     * VOBJ: Returns a reference to the value at the numeric index (regardless of key),
     *       or throws std::out_of_range if index >= object size.
     * VARR: Returns a reference to the element at the index,
     *       or throws std::out_of_range if index >= array size.
     * Other types: Throws std::domain_error.
     *
     * The returned reference follows the iterator invalidation rules of the underlying vector.
     *
     * Complexity: constant.
     *
     * This is a Radiant Node extension of the UniValue API.
     *
     * If you don't want an exception thrown, please use the [] operator instead.
     */
    const UniValue& at(size_type index) const;
    UniValue& at(size_type index);

    /**
     * Returns whether the value's type is the same as the given type.
     * If a type bitmask is given, returns whether the value's type is any of the given types.
     *
     * Complexity: constant.
     *
     * This is a Radiant Node extension of the UniValue API.
     */
    [[nodiscard]]
    constexpr bool is(int types) const noexcept { return typ & types; }

    [[nodiscard]]
    constexpr bool isNull() const noexcept { return is(VNULL); }
    [[nodiscard]]
    constexpr bool isFalse() const noexcept { return is(VFALSE); }
    [[nodiscard]]
    constexpr bool isTrue() const noexcept { return is(VTRUE); }
    [[nodiscard]]
    constexpr bool isBool() const noexcept { return is(MBOOL); }
    [[nodiscard]]
    constexpr bool isObject() const noexcept { return is(VOBJ); }
    [[nodiscard]]
    constexpr bool isArray() const noexcept { return is(VARR); }
    [[nodiscard]]
    constexpr bool isNum() const noexcept { return is(VNUM); }
    [[nodiscard]]
    constexpr bool isStr() const noexcept { return is(VSTR); }

    /**
     * Returns the JSON string representation of the provided value.
     *
     * The type of value can be the generic UniValue,
     * or a more specific type: UniValue::Object, UniValue::Array, or std::string.
     *
     * The optional argument indicates the number of spaces for indentation in pretty formatting.
     * Use 0 (default) to disable pretty formatting and use compact formatting instead.
     * Note that pretty formatting only affects arrays and objects.
     *
     * This is a Radiant Node extension of the UniValue API.
     */
    template<typename Value>
    static std::string stringify(const Value& value, unsigned int prettyIndent = 0) {
        std::string s; // we do it this way for RVO to work on all compilers
        Stream ss{s};
        s.reserve(1024);
        stringify(ss, value, prettyIndent, 0);
        return s;
    }

    /**
     * Parses a NUL-terminated JSON string.
     *
     * If valid JSON, a pointer to the terminating NUL of the JSON is returned, and the object state represents the read value.
     * If invalid JSON, nullptr is returned, and the object is in a valid but unspecified state.
     *
     * (If the return value is cast to bool, the resulting boolean indicates success.)
     *
     * Compatible with the upstream UniValue API, although upstream simply returns a bool.
     */
    [[nodiscard]]
    const char* read(const char* raw);

    /**
     * Parses a JSON std::string.
     *
     * If valid JSON, true is returned, and the object state represents the read value.
     * If invalid JSON, false is returned, and the object is in a valid but unspecified state.
     *
     * Compatible with the upstream UniValue API.
     */
    [[nodiscard]]
    bool read(const std::string& raw);

private:
    UniValue::VType typ = VNULL;
    std::string val;                       // numbers are stored as C++ strings
    Object entries;
    Array values;

    // Opaque type used for writing. This can be further optimized later.
    struct Stream {
        std::string & str; // this is a reference for RVO to always work in UniValue::stringify()
        void put(char c) { str.push_back(c); }
        void put(char c, size_t nFill) { str.append(nFill, c); }
        Stream & operator<<(std::string_view s) { str.append(s); return *this; }
    };
    static inline void startNewLine(Stream & stream, unsigned int prettyIndent, unsigned int indentLevel);
    static void jsonEscape(Stream & stream, std::string_view inString);

    static void stringify(Stream & stream, const UniValue& value, unsigned int prettyIndent, unsigned int indentLevel);
    static void stringify(Stream & stream, const UniValue::Object& value, unsigned int prettyIndent, unsigned int indentLevel);
    static void stringify(Stream & stream, const UniValue::Array& value, unsigned int prettyIndent, unsigned int indentLevel);
    static void stringify(Stream & stream, std::string_view value, unsigned int prettyIndent, unsigned int indentLevel);

    // Used internally by the integral operator=() overloads
    template<typename Int64>
    void setInt64(Int64 val);

public:
    // Strict type-specific getters, these throw std::runtime_error if the
    // value is of unexpected type

    bool get_bool() const;
    int get_int() const;
    int64_t get_int64() const;
    double get_real() const;

    /**
     * VSTR: Returns a std::string reference to this value.
     * Other types: Throws std::runtime_error.
     *
     * Destroying the string (e.g. destroying the UniValue wrapper or
     * assigning a different type to it) invalidates the returned reference.
     *
     * Complexity: constant.
     *
     * Compatible with the upstream UniValue API.
     * Non-const overload is a Radiant Node extension.
     */
    const std::string& get_str() const;
    std::string& get_str();

    /**
     * VOBJ: Returns a UniValue::Object reference to this value.
     * Other types: Throws std::runtime_error.
     *
     * Destroying the object (e.g. destroying the UniValue wrapper or
     * assigning a different type to it) invalidates the returned reference.
     *
     * Complexity: constant.
     *
     * Compatible with the upstream UniValue API,
     * but with a different return type.
     * Non-const overload is a Radiant Node extension.
     */
    const Object& get_obj() const;
    Object& get_obj();

    /**
     * VARR: Returns a UniValue::Array reference to this value.
     * Other types: Throws std::runtime_error.
     *
     * Destroying the array (e.g. destroying the UniValue wrapper or
     * assigning a different type to it) invalidates the returned reference.
     *
     * Complexity: constant.
     *
     * Compatible with the upstream UniValue API,
     * but with a different return type.
     * Non-const overload is a Radiant Node extension.
     */
    const Array& get_array() const;
    Array& get_array();

    [[nodiscard]]
    constexpr VType type() const noexcept { return getType(); }
};

enum jtokentype {
    JTOK_ERR        = -1,
    JTOK_NONE       = 0,                           // eof
    JTOK_OBJ_OPEN,
    JTOK_OBJ_CLOSE,
    JTOK_ARR_OPEN,
    JTOK_ARR_CLOSE,
    JTOK_COLON,
    JTOK_COMMA,
    JTOK_KW_NULL,
    JTOK_KW_TRUE,
    JTOK_KW_FALSE,
    JTOK_NUMBER,
    JTOK_STRING,
};

extern jtokentype getJsonToken(std::string& tokenVal, const char*& buffer);

/**
 * Returns the human-readable name of the JSON value type.
 *
 * Compatible with the upstream UniValue API (but note VBOOL has been replaced with VFALSE and VTRUE).
 */
[[nodiscard]]
extern const char *uvTypeName(UniValue::VType t) noexcept;

/**
 * Returns the human-readable name of the composite JSON value type bitmask.
 * Argument must be a bitmask consisting of one or more UniValue::VType elements.
 *
 * This is a Radiant Node extension of the UniValue API.
 */
[[nodiscard]]
extern std::string uvTypeName(int t);

[[nodiscard]]
inline constexpr bool jsonTokenIsValue(jtokentype jtt) noexcept
{
    switch (jtt) {
    case JTOK_KW_NULL:
    case JTOK_KW_TRUE:
    case JTOK_KW_FALSE:
    case JTOK_NUMBER:
    case JTOK_STRING:
        return true;

    default:
        return false;
    }

    // not reached
}

[[nodiscard]]
inline constexpr bool json_isspace(int ch) noexcept
{
    switch (ch) {
    case 0x20:
    case 0x09:
    case 0x0a:
    case 0x0d:
        return true;

    default:
        return false;
    }

    // not reached
}

extern const UniValue NullUniValue;

#endif // __UNIVALUE_H__
