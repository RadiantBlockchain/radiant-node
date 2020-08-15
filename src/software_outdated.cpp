// Copyright (c) 2020 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <software_outdated.h>

#include <clientversion.h>
#include <scheduler.h>
#include <util/system.h>
#include <util/time.h>

#include <algorithm>
#include <cmath>


namespace software_outdated {
//! When in advance to start warning that the software will become outdated (30 days in seconds).
static constexpr int64_t WARN_LEAD_TIME = 3600 * 24 * 30;
int64_t nTime = 0;
bool fDisableRPCOnExpiry{false};
std::atomic_bool fRPCDisabled{false};

bool IsOutdated() {
    return nTime > 0 && GetTime() + WARN_LEAD_TIME >= nTime;
}

bool IsExpired() {
    return nTime > 0 && GetTime() >= nTime;
}

std::string GetWarnString(bool translated) {
    std::string ret;
    const auto delta = nTime > 0 ? nTime - GetTime() : int64_t(0);
    const auto nDaysUntil = int64_t(std::round(delta / (60.*60.*24.)));
    // This lambda is here so that the translation scripts pick up the string,
    // but we can control whether or not to actually translate it.
    const auto _ = [translated](const char * const s) -> std::string {
        return translated ? ::_(s) : s;
    };
    if (nDaysUntil >= 0 && delta > 0) {
        ret = strprintf(_("Warning: This version of %s is old and may fall out"
                          " of network consensus in %d day(s). Please upgrade,"
                          " or add expire=0 to your configuration file if you"
                          " want to continue running this version. If you do"
                          " nothing, the software will gracefully degrade by"
                          " limiting its functionality in %d day(s)."),
                        CLIENT_NAME, nDaysUntil, nDaysUntil);
    } else {
        ret = strprintf(_("Warning: This version of %s is old and may have"
                          " fallen out of network consensus %d day(s) ago."
                          " Please upgrade, or add expire=0 to your"
                          " configuration file."),
                        CLIENT_NAME, -nDaysUntil);
    }
    return ret;
}

std::string GetRPCDisabledString() {
    return strprintf("RPC is disabled. This version of %s is old and may be"
                     " out of consensus with the network. It is recommended"
                     " that you upgrade. To proceed without upgrading, and"
                     " re-enable the RPC interface, restart the node with"
                     " the configuration option expire=0.", CLIENT_NAME);
}

void SetupExpiryHooks(CScheduler &scheduler) {
    if (!nTime) {
        // log printing is disabled (CLI arg -expire=0)
        return;
    }

    //! If we are already outdated, print the outdated warning to the log
    //! in 1 second, then proceed every hour.
    static constexpr int64_t WARN_LOG_IMMEDIATE_TIME = 1;
    //! Print the outdated warning to the log every hour
    static constexpr int64_t WARN_LOG_INTERVAL = 60 * 60;

    static const auto Task = [] {
        // print to the log if we are really still outdated
        // we may become un-outdated only if the user's clock
        // was reset back.
        if (IsOutdated())
            LogPrintf("\n\n%s\n\n", GetWarnString(false));
        // disable RPC if we are at or past the expiry deadline
        fRPCDisabled = fDisableRPCOnExpiry && IsExpired();
    };
    const auto InstallTask = [&scheduler] {
        // print to log in 1 second
        scheduler.scheduleFromNow(Task, WARN_LOG_IMMEDIATE_TIME * 1000);
        // print to log every hour
        scheduler.scheduleEvery([]{Task(); return true;}, WARN_LOG_INTERVAL * 1000);
        const auto secondsFromNowExpiry = nTime - GetTime();
        if (secondsFromNowExpiry > WARN_LOG_IMMEDIATE_TIME && secondsFromNowExpiry != WARN_LOG_INTERVAL) {
            // schedule the task to also fire exactly 500ms after we expire
            // (if it's in the future)
            scheduler.scheduleFromNow(Task, secondsFromNowExpiry * 1000 + 500);
        }
    };
    if (IsOutdated()) {
        // already outdated (or possibly even expired) -- schedule the task now
        InstallTask();
    } else {
        // precisely when we will become "outdated"
        const auto secondsFromNow = (nTime - WARN_LEAD_TIME) - GetTime();
        // not outdated yet -- schedule the "InstallTask" lambda to fire
        // when we will become IsOutdated() in the futre.
        // ensure this is a future time.
        const auto millisFromNow = std::max(secondsFromNow * 1000, int64_t(100));
        scheduler.scheduleFromNow(InstallTask, millisFromNow);
    }
}

} // namespace software_outdated
