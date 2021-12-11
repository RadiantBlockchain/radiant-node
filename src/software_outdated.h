// Copyright (c) 2020 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <atomic>
#include <cstdint>
#include <string>

class CScheduler;

namespace software_outdated {

//! Default for CLI arg -expire, which controls this facility
static constexpr bool DEFAULT_EXPIRE = true;

//! Default for CLI arg -expirerpc, which controls whether we
//! disable RPC when we enter the "expired" state
static constexpr bool DEFAULT_EXPIRE_RPC = true;

//! This is set at startup by init.cpp. This is normally the unix time at which
//! the next tentative network upgrade will occur that we do not yet have
//! consensus rules for. If this value is <= 0, outdated warnings are disabled,
//! as is the RPC disable mechanism.
extern int64_t nTime;

//! If true, we will disable RPC when we enter the "expired" state.
//! This is set at startup by init.cpp.
extern bool fDisableRPCOnExpiry;

//! If true, we are expired and the RPC service will fail for everything but
//! the "stop" command. rpc/server.cpp reads this flag.
extern std::atomic_bool fRPCDisabled;

//! Returns true if we are in the "software outdated" state, false otherwise.
//! We enter the "software outdated" state 30 days before nTime (if nTime is
//! nonzero). If nTime is zero this will always return false. Note that
//! we can be both expired and outdated.
bool IsOutdated();

//! Returns true if we are in the "expired" state (nTime has already passed).
//! If nTime is zero this will always return false. Note that we can be both
//! expired and outdated.
bool IsExpired();

//! Adds a scheduled task to the scheduler that will print GetWarnString()
//! to the log every hour, starting 30 days before the software is due to
//! become outdated. If the software is already outdated, the printing starts
//! within 1 second and then proceeds to happen every hour thereafter.
//! This function is a no-op if nTime == 0.
void SetupExpiryHooks(CScheduler &scheduler);

//! Returns the "software outdated" warning string suitable for printing to the
//! log. Call this only if IsSoftwareOutdated()
std::string GetWarnString(bool translated=false);

//! Returns the "software outdated" error string for when the RPC service is
//! disabled. Call this only if fExpiredRPC is set. Called by rpc/server.cpp.
std::string GetRPCDisabledString();

} // namespace software_outdated
