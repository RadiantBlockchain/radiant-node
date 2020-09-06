// Copyright 2014 BitPay Inc.
// Copyright 2015 Bitcoin Core Developers
// Copyright (c) 2020 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef __UNIVALUE_H__
#define __UNIVALUE_H__

#include <stdint.h>
#include <string.h>

#include <cassert>
#include <map>
#include <string>
#include <utility>
#include <vector>

class UniValue {
public:
    enum VType { VNULL, VOBJ, VARR, VSTR, VNUM, VBOOL, };

    using Object = std::vector<std::pair<std::string, UniValue>>;
    using Array = std::vector<UniValue>;

    UniValue(UniValue::VType initialType = VNULL) noexcept : typ(initialType) {}
    UniValue(UniValue::VType initialType, const std::string& initialStr)
        : typ(initialType), val(initialStr) {}
    UniValue(UniValue::VType initialType, std::string&& initialStr) noexcept
        : typ(initialType), val(std::move(initialStr)) {}
    UniValue(uint64_t val_) { setInt(val_); }
    UniValue(int64_t val_) { setInt(val_); }
    UniValue(bool val_) { setBool(val_); }
    UniValue(int val_) { setInt(val_); }
    UniValue(double val_) { setFloat(val_); }
    UniValue(const std::string& val_) : typ(VSTR), val(val_) {}
    UniValue(std::string&& val_) noexcept : typ(VSTR), val(std::move(val_)) {}
    UniValue(const char *val_) : typ(VSTR), val(val_) {}
    UniValue(const Array& array) : typ(VARR), values(array) {}
    UniValue(Array&& array) : typ(VARR), values(std::move(array)) {}
    UniValue(const Object& object) : typ(VOBJ), entries(object) {}
    UniValue(Object&& object) : typ(VOBJ), entries(std::move(object)) {}

    void setNull() noexcept;
    void setBool(bool val);
    void setNumStr(const std::string& val);
    void setNumStr(std::string&& val) noexcept;
    void setInt(uint64_t val);
    void setInt(int64_t val);
    void setInt(int val_) { setInt(int64_t(val_)); }
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
    size_t size() const noexcept {
        switch (typ) {
        case VOBJ:
            return entries.size();
        case VARR:
            return values.size();
        default:
            return 0;
        }
    }

    /**
     * VOBJ/VARR: Increases the capacity of the underlying vector to at least n.
     * Other types: Does nothing.
     *
     * Complexity: at most linear in number of elements.
     *
     * Compatible with the upstream UniValue API for VOBJ/VARR but does not implement upstream behavior for other types.
     */
    void reserve(size_t n);

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
     * If you want an exception thrown on missing keys, please use at() instead.
     */
    const UniValue& operator[](size_t index) const noexcept;

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
    const UniValue& at(size_t index) const;
    UniValue& at(size_t index);

    constexpr bool isNull() const noexcept { return typ == VNULL; }
    constexpr bool isTrue() const noexcept { return typ == VBOOL && val == boolTrueVal; }
    constexpr bool isFalse() const noexcept { return typ == VBOOL && val != boolTrueVal; }
    constexpr bool isBool() const noexcept { return typ == VBOOL; }
    constexpr bool isStr() const noexcept { return typ == VSTR; }
    constexpr bool isNum() const noexcept { return typ == VNUM; }
    constexpr bool isArray() const noexcept { return typ == VARR; }
    constexpr bool isObject() const noexcept { return typ == VOBJ; }

    void push_back(UniValue&& val);
    void push_back(const UniValue& val);

    // checkForDupes=true is slower, but does a linear search through the keys to overwrite existing keys.
    // checkForDupes=false is faster, and will always append the new entry at the end (even if `key` exists).
    void pushKV(const std::string& key, const UniValue& val, bool checkForDupes = true);
    void pushKV(const std::string& key, UniValue&& val, bool checkForDupes = true);
    void pushKV(std::string&& key, const UniValue& val, bool checkForDupes = true);
    void pushKV(std::string&& key, UniValue&& val, bool checkForDupes = true);

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
    template<typename Value> static std::string stringify(const Value& value, unsigned int prettyIndent = 0) {
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
    UniValue::VType typ;
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

    // Used by the various setInt() and setFloat() overloads
    template<typename Num>
    void setIntOrFloat(Num numVal);

public:
    // Strict type-specific getters, these throw std::runtime_error if the
    // value is of unexpected type

    /**
     * VOBJ: Returns a reference to the underlying vector of key-value pairs.
     * Other types: Throws std::runtime_error.
     *
     * Destroying the object invalidates the returned reference.
     *
     * Complexity: constant.
     *
     * This is a Bitcoin Cash Node extension of the UniValue API.
     */
    const Object& getObjectEntries() const;
    Object& getObjectEntries();

    /**
     * VARR: Returns a reference to the underlying vector of values.
     * Other types: Throws std::runtime_error.
     *
     * Destroying the array invalidates the returned reference.
     *
     * Complexity: constant.
     *
     * This is a Bitcoin Cash Node extension of the UniValue API.
     *
     * If you want to clear the array after using this method, consider using takeArrayValues() instead.
     */
    const Array& getArrayValues() const;
    Array& getArrayValues();

    /**
     * VARR: Changes the UniValue into an empty array and returns the old array contents as a vector.
     * Other types: Throws std::runtime_error.
     *
     * Complexity: constant.
     *
     * This is a Bitcoin Cash Node extension of the UniValue API.
     *
     * If you do not want to make the array empty, please use getArrayValues() instead.
     */
    Array takeArrayValues();

    bool get_bool() const;
    const std::string& get_str() const;
    int get_int() const;
    int64_t get_int64() const;
    double get_real() const;
    const UniValue& get_obj() const;
    const UniValue& get_array() const;

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
