// Copyright 2014 BitPay Inc.
// Copyright 2015 Bitcoin Core Developers
// Copyright (c) 2020 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef __UNIVALUE_H__
#define __UNIVALUE_H__

#include <cassert>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

class UniValue {
public:
    enum VType { VNULL, VOBJ, VARR, VSTR, VNUM, VBOOL, };

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
        const_iterator begin() const noexcept { return vector.begin(); }
        iterator begin() noexcept { return vector.begin(); }

        /**
         * Returns an iterator to the past-the-last key-value pair of the object.
         *
         * The returned iterator follows the iterator invalidation rules of the underlying vector.
         *
         * Complexity: constant.
         */
        const_iterator end() const noexcept { return vector.end(); }
        iterator end() noexcept { return vector.end(); }

        /**
         * Returns an iterator to the first key-value pair of the reversed object.
         *
         * The returned iterator follows the iterator invalidation rules of the underlying vector.
         *
         * Complexity: constant.
         */
        const_reverse_iterator rbegin() const noexcept { return vector.rbegin(); }
        reverse_iterator rbegin() noexcept { return vector.rbegin(); }

        /**
         * Returns an iterator to the past-the-last key-value pair of the reversed object.
         *
         * The returned iterator follows the iterator invalidation rules of the underlying vector.
         *
         * Complexity: constant.
         */
        const_reverse_iterator rend() const noexcept { return vector.rend(); }
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
        bool empty() const noexcept { return vector.empty(); }

        /**
         * Returns the size of the object.
         *
         * Complexity: constant.
         */
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
        const UniValue& operator[](const std::string& key) const noexcept;

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
        const UniValue* locate(const std::string& key) const noexcept;
        UniValue* locate(const std::string& key) noexcept;

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
        const UniValue& at(const std::string& key) const;
        UniValue& at(const std::string& key);

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
        const UniValue& front() const noexcept;

        /**
         * Returns a reference to the last value (regardless of key),
         * or NullUniValue if the object is empty.
         *
         * The returned reference follows the iterator invalidation rules of the underlying vector.
         *
         * Complexity: constant.
         */
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
        bool operator==(const Object& other) const noexcept { return vector == other.vector; }

        /**
         * Returns whether the objects contain unequal data.
         * Two objects are not considered equal if elements are ordered differently.
         *
         * Complexity: linear in the amount of data to compare.
         */
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
        const_iterator begin() const noexcept { return vector.begin(); }
        iterator begin() noexcept { return vector.begin(); }

        /**
         * Returns an iterator to the past-the-last value of the array.
         *
         * The returned iterator follows the iterator invalidation rules of the underlying vector.
         *
         * Complexity: constant.
         */
        const_iterator end() const noexcept { return vector.end(); }
        iterator end() noexcept { return vector.end(); }

        /**
         * Returns an iterator to the first value of the reversed array.
         *
         * The returned iterator follows the iterator invalidation rules of the underlying vector.
         *
         * Complexity: constant.
         */
        const_reverse_iterator rbegin() const noexcept { return vector.rbegin(); }
        reverse_iterator rbegin() noexcept { return vector.rbegin(); }

        /**
         * Returns an iterator to the past-the-last value of the reversed array.
         *
         * The returned iterator follows the iterator invalidation rules of the underlying vector.
         *
         * Complexity: constant.
         */
        const_reverse_iterator rend() const noexcept { return vector.rend(); }
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
        bool empty() const noexcept { return vector.empty(); }

        /**
         * Returns the size of the array.
         *
         * Complexity: constant.
         */
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
        const UniValue& front() const noexcept;

        /**
         * Returns a reference to the last value,
         * or NullUniValue if the array is empty.
         *
         * The returned reference follows the iterator invalidation rules of the underlying vector.
         *
         * Complexity: constant.
         */
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
        bool operator==(const Array& other) const noexcept { return vector == other.vector; }

        /**
         * Returns whether the arrays contain unequal data.
         *
         * Complexity: linear in the amount of data to compare.
         */
        bool operator!=(const Array& other) const noexcept { return !(*this == other); }
    };

    using size_type = Object::size_type;
    static_assert(std::is_same<size_type, Array::size_type>::value,
                  "UniValue::size_type should be equal to both UniValue::Object::size_type and UniValue::Array::size_type.");

    explicit UniValue(VType initialType = VNULL) noexcept : typ(initialType) {}
    UniValue(VType initialType, const std::string& initialStr) : typ(initialType), val(initialStr) {}
    UniValue(VType initialType, std::string&& initialStr) noexcept : typ(initialType), val(std::move(initialStr)) {}
    UniValue(bool val_) { setBool(val_); }
    UniValue(short val_) { setInt(val_); }
    UniValue(int val_) { setInt(val_); }
    UniValue(long val_) { setInt(val_); }
    UniValue(long long val_) { setInt(val_); }
    UniValue(unsigned short val_) { setInt(val_); }
    UniValue(unsigned val_) { setInt(val_); }
    UniValue(unsigned long val_) { setInt(val_); }
    UniValue(unsigned long long val_) { setInt(val_); }
    UniValue(double val_) { setFloat(val_); }
    UniValue(const std::string& val_) : typ(VSTR), val(val_) {}
    UniValue(std::string&& val_) noexcept : typ(VSTR), val(std::move(val_)) {}
    UniValue(const char *val_) : typ(VSTR), val(val_) {}
    explicit UniValue(const Array& array) : typ(VARR), values(array) {}
    UniValue(Array&& array) : typ(VARR), values(std::move(array)) {}
    explicit UniValue(const Object& object) : typ(VOBJ), entries(object) {}
    UniValue(Object&& object) : typ(VOBJ), entries(std::move(object)) {}
    explicit UniValue(const UniValue&) = default;
    UniValue(UniValue&&) noexcept = default;
    UniValue& operator=(const UniValue&) = default;
    UniValue& operator=(UniValue&&) = default;

    void setNull() noexcept;
    void setBool(bool val);
    void setNumStr(const std::string& val);
    void setNumStr(std::string&& val) noexcept;
    void setInt(short val_);
    void setInt(int val_);
    void setInt(long val_);
    void setInt(long long val_);
    void setInt(unsigned short val_);
    void setInt(unsigned val_);
    void setInt(unsigned long val_);
    void setInt(unsigned long long val_);
    void setFloat(double val);
    void setStr(const std::string& val);
    void setStr(std::string&& val) noexcept;
    void setArray() noexcept;
    void setArray(const Array& array);
    void setArray(Array&& array) noexcept;
    void setObject() noexcept;
    void setObject(const Object& object);
    void setObject(Object&& object) noexcept;

    constexpr enum VType getType() const noexcept { return typ; }
    constexpr const std::string& getValStr() const noexcept { return val; }

    /**
     * VOBJ/VARR: Returns whether the object/array is empty.
     * Other types: Returns true.
     *
     * Complexity: constant.
     *
     * Compatible with the upstream UniValue API.
     */
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
    const UniValue& operator[](const std::string& key) const noexcept;

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
    const UniValue& operator[](size_type index) const noexcept;

    /**
     * Returns whether the UniValues are of the same type and contain equal data.
     * Two objects/arrays are not considered equal if elements are ordered differently.
     *
     * Complexity: linear in the amount of data to compare.
     *
     * This is a Bitcoin Cash Node extension of the UniValue API.
     */
    bool operator==(const UniValue& other) const noexcept;

    /**
     * Returns whether the UniValues are not of the same type or contain unequal data.
     * Two objects/arrays are not considered equal if elements are ordered differently.
     *
     * Complexity: linear in the amount of data to compare.
     *
     * This is a Bitcoin Cash Node extension of the UniValue API.
     */
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
     * This is a Bitcoin Cash Node extension of the UniValue API.
     */
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
     * This is a Bitcoin Cash Node extension of the UniValue API.
     */
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
     * This is a Bitcoin Cash Node extension of the UniValue API.
     *
     * If you want to treat missing keys as null values, please use the [] operator instead.
     * If you want an exception thrown on missing keys, please use at() instead.
     */
    const UniValue* locate(const std::string& key) const noexcept;
    UniValue* locate(const std::string& key) noexcept;

    /**
     * VOBJ: Returns a reference to the first value associated with the key,
     *       or throws std::out_of_range if the key does not exist.
     * Other types: Throws std::domain_error.
     *
     * The returned reference follows the iterator invalidation rules of the underlying vector.
     *
     * Complexity: linear in number of elements.
     *
     * This is a Bitcoin Cash Node extension of the UniValue API.
     *
     * If you don't want an exception thrown, please use locate() or the [] operator instead.
     */
    const UniValue& at(const std::string& key) const;
    UniValue& at(const std::string& key);

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
     * This is a Bitcoin Cash Node extension of the UniValue API.
     *
     * If you don't want an exception thrown, please use the [] operator instead.
     */
    const UniValue& at(size_type index) const;
    UniValue& at(size_type index);

    constexpr bool isNull() const noexcept { return typ == VNULL; }
    constexpr bool isTrue() const noexcept { return typ == VBOOL && val == boolTrueVal; }
    constexpr bool isFalse() const noexcept { return typ == VBOOL && val != boolTrueVal; }
    constexpr bool isBool() const noexcept { return typ == VBOOL; }
    constexpr bool isStr() const noexcept { return typ == VSTR; }
    constexpr bool isNum() const noexcept { return typ == VNUM; }
    constexpr bool isArray() const noexcept { return typ == VARR; }
    constexpr bool isObject() const noexcept { return typ == VOBJ; }

    /**
     * Returns the JSON string representation of the provided value.
     *
     * The type of value can be the generic UniValue,
     * or a more specific type: bool, std::string, UniValue::Array, or UniValue::Object.
     *
     * The optional argument indicates the number of spaces for indentation in pretty formatting.
     * Use 0 (default) to disable pretty formatting and use compact formatting instead.
     * Note that pretty formatting only affects arrays and objects.
     *
     * This is a Bitcoin Cash Node extension of the UniValue API.
     */
    template<typename Value>
    static std::string stringify(const Value& value, unsigned int prettyIndent = 0) {
        std::string s; // we do it this way for RVO to work on all compilers
        Stream ss{s};
        s.reserve(1024);
        stringify(ss, value, prettyIndent, 0);
        return s;
    }

    bool read(const char *raw, size_t len);
    bool read(const char *raw) { return read(raw, strlen(raw)); }
    bool read(const std::string& rawStr) { return read(rawStr.data(), rawStr.size()); }

private:
    UniValue::VType typ = VNULL;
    std::string val;                       // numbers are stored as C++ strings
    Object entries;
    Array values;
    static const std::string boolTrueVal; // = "1"

    // Opaque type used for writing. This can be further optimized later.
    struct Stream {
        std::string & str; // this is a reference for RVO to always work in UniValue::stringify()
        void put(char c) { str.push_back(c); }
        void put(char c, size_t nFill) { str.append(nFill, c); }
        void write(const char *s, size_t len) { str.append(s, len); }
        Stream & operator<<(const char *s) { str.append(s); return *this; }
        Stream & operator<<(const std::string &s) { str.append(s); return *this; }
    };
    static inline void startNewLine(Stream & stream, unsigned int prettyIndent, unsigned int indentLevel);
    static void jsonEscape(Stream & stream, const std::string & inString);

    static void stringify(Stream & stream, const UniValue& value, unsigned int prettyIndent, unsigned int indentLevel);
    static void stringify(Stream & stream, bool value, unsigned int prettyIndent, unsigned int indentLevel);
    static void stringify(Stream & stream, const std::string& value, unsigned int prettyIndent, unsigned int indentLevel);
    static void stringify(Stream & stream, const UniValue::Array& value, unsigned int prettyIndent, unsigned int indentLevel);
    static void stringify(Stream & stream, const UniValue::Object& value, unsigned int prettyIndent, unsigned int indentLevel);

    // Used by the setInt() overloads
    template<typename Integer>
    void setInt64(Integer val);

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
     * Non-const overload is a Bitcoin Cash Node extension.
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
     * Non-const overload is a Bitcoin Cash Node extension.
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
     * Non-const overload is a Bitcoin Cash Node extension.
     */
    const Array& get_array() const;
    Array& get_array();

    constexpr enum VType type() const noexcept { return getType(); }
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

extern enum jtokentype getJsonToken(std::string& tokenVal,
                                    unsigned int& consumed, const char *raw, const char *end);
extern const char *uvTypeName(UniValue::VType t) noexcept;

static constexpr bool jsonTokenIsValue(enum jtokentype jtt) noexcept
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

static constexpr bool json_isspace(int ch) noexcept
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
