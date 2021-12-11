// Copyright (C) 2019-2020 Tom Zander <tomz@freedommail.ch>
// Copyright (C) 2020-2021 Calin Culianu <calin.culianu@gmail.com>
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#pragma once

#include <uint256.h>

#include <memory>


//! Unique identifier for a DoubleSpend proof.
//! It is just the hash of the serialized data structure.
using DspId = uint256;


//! A unique_ptr<DspId> work-alike that supports copy construction/assignment.
//!
//! Notes:
//! - Copying/assigning creates a deep-copy (duplicate) of the underlying DspId
//! - Copying/assigning of a DspId that .IsNull() will result in this class
//!   holding a nullptr, as a memory saving technique, since .IsNull() DspId's
//!   are semantically equivalent to a DspId that is not valid and/or does not
//!   exist.
//! - Unlike unique_ptr, comaprison operators always do a deep-compare of the
//!   pointed-to DspId, with nullptr being semantically equivalend to an IsNull()
//!   DspId. Comparing nullptr DspIdPtrs is supported.
//!
//! This class is intended to be used as an instance member for long-lived
//! types such as CTxMemPoolEntry. Use of this class saves memory in the
//! common-case of no DspId associated with the instance.
//!
//! This class takes only 8 bytes of memory on 64-bit, in the common-case of
//! no valid DspId, as compared to a direct DspId instance which would
//! take 32 bytes always, even if there is no associated double-spend proof.
class DspIdPtr final {
    std::unique_ptr<DspId> p;
    void copy(const DspId *o) {
        if (const bool valid = o && !o->IsNull(); p && valid) {
            // deep copy onto the already-allocated DspId
            *p = *o;
         } else {
            // allocate a new unique_ptr if `o` is `valid`, otherwise just use nullptr
            p = valid ? std::make_unique<DspId>(*o) : nullptr;
        }
    }
public:
    constexpr DspIdPtr() noexcept = default;
    //! Support conversion construction.
    //! Note that if dspId.IsNull() then this instance .get() will be nullptr
    DspIdPtr(const DspId &dspId) { copy(&dspId); }
    //! Support copy-construction (unlike unique_ptr which does not)
    DspIdPtr(const DspIdPtr &o) { copy(o.p.get()); }
    DspIdPtr(DspIdPtr &&o) noexcept : p(std::move(o.p)) {}
    //! Support copy-assignment (unlike unique_ptr which does not)
    DspIdPtr &operator=(const DspIdPtr &o) {
        copy(o.p.get());
        return *this;
    }
    DspIdPtr &operator=(DspIdPtr &&o) noexcept { p = std::move(o.p); return *this; }
    //! Convenience: Support copy-assignemnt from the underling type directly
    DspIdPtr &operator=(const DspId &d) {
        copy(&d);
        return *this;
    }

    // -- comparison operators --
    //! Convenience: Compare for equality directly to a DspId
    bool operator==(const DspId &d) const {
        if (p) {
            return *p == d;
        } else {
            return d.IsNull();
        }
    }
    bool operator!=(const DspId &d) const { return !((*this) == d); }
    bool operator<(const DspId &d) const {
        if (p) {
            return *p < d;
        } else {
            return DspId{} < d;
        }
    }
    bool operator<=(const DspId &d) const { return (*this) < d || (*this) == d; }
    bool operator>(const DspId &d) const { return !((*this) <= d); }
    bool operator>=(const DspId &d) const { return !((*this) < d); }

    // Note: unlike unique_ptr operator<=> comparisons, the below compare the underlying DspId (deep comparison)
    bool operator==(const DspIdPtr &o) const { return *this == (o.p ? *o.p : DspId{});}
    bool operator!=(const DspIdPtr &o) const { return *this != (o.p ? *o.p : DspId{});}
    bool operator<(const DspIdPtr &o) const { return *this < (o.p ? *o.p : DspId{});}
    bool operator<=(const DspIdPtr &o) const { return *this <= (o.p ? *o.p : DspId{});}
    bool operator>(const DspIdPtr &o) const { return *this > (o.p ? *o.p : DspId{});}
    bool operator>=(const DspIdPtr &o) const { return *this >= (o.p ? *o.p : DspId{});}

    // -- unique_ptr work-alike methods --
    explicit operator bool() const noexcept { return bool(p); }
    DspId & operator*() { return *p; }
    const DspId & operator*() const { return *p; }
    DspId * operator->() { return p.get(); }
    const DspId * operator->() const { return p.get(); }
    DspId * get() { return p.get(); }
    const DspId * get() const { return p.get(); }
    void reset() { p.reset(); }

    // used by tests
    std::size_t memUsage() const { return sizeof(*this) + (p ? sizeof(*p) : 0); }
};
