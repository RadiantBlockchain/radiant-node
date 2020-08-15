// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Copyright (c) 2020 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <warnings.h>

#include <clientversion.h>
#include <software_outdated.h>
#include <sync.h>
#include <util/system.h>

RecursiveMutex cs_warnings;
std::string strMiscWarning GUARDED_BY(cs_warnings);
bool fLargeWorkForkFound GUARDED_BY(cs_warnings) = false;
bool fLargeWorkInvalidChainFound GUARDED_BY(cs_warnings) = false;

void SetMiscWarning(const std::string &strWarning) {
    LOCK(cs_warnings);
    strMiscWarning = strWarning;
}

void SetfLargeWorkForkFound(bool flag) {
    LOCK(cs_warnings);
    fLargeWorkForkFound = flag;
}

bool GetfLargeWorkForkFound() {
    LOCK(cs_warnings);
    return fLargeWorkForkFound;
}

void SetfLargeWorkInvalidChainFound(bool flag) {
    LOCK(cs_warnings);
    fLargeWorkInvalidChainFound = flag;
}

std::string GetWarnings(const std::string &strFor) {
    std::string strStatusBar;
    std::string strGUI;
    const std::string uiAlertSeperator = "<hr />";

    LOCK(cs_warnings);

    if (!CLIENT_VERSION_IS_RELEASE) {
        strStatusBar = "This is a pre-release test build - use at your own "
                       "risk - do not use for mining or merchant applications";
        strGUI = _("This is a pre-release test build - use at your own risk - "
                   "do not use for mining or merchant applications");
    }

    // Misc warnings like out of disk space and clock is wrong
    if (strMiscWarning != "") {
        strStatusBar = strMiscWarning;
        strGUI += (strGUI.empty() ? "" : uiAlertSeperator) + strMiscWarning;
    }

    const bool fOutDated = software_outdated::IsOutdated();

    if (fLargeWorkForkFound) {
        strStatusBar = "Warning: The network does not appear to fully agree! "
                       "Some miners appear to be experiencing issues.";
        strGUI += (strGUI.empty() ? "" : uiAlertSeperator) +
                  _("Warning: The network does not appear to fully agree! Some "
                    "miners appear to be experiencing issues.");
    } else if (fLargeWorkInvalidChainFound) {
        strStatusBar = "Warning: We do not appear to fully agree with our "
                       "peers! You may need to upgrade, or other nodes may "
                       "need to upgrade.";
        strGUI +=
            (strGUI.empty() ? "" : uiAlertSeperator) +
            _("Warning: We do not appear to fully agree with our peers! You "
              "may need to upgrade, or other nodes may need to upgrade.");
    } else if (fOutDated) {
        strStatusBar = software_outdated::GetWarnString(false /* translated */);
    }

    if (fOutDated) {
        // Unconditionally add this warning to the GUI if we are outdated
        strGUI += (strGUI.empty() ? "" : uiAlertSeperator)
                  + software_outdated::GetWarnString(true /* translated */);
    }

    if (strFor == "gui") {
        return strGUI;
    } else if (strFor == "statusbar") {
        return strStatusBar;
    }
    assert(!"GetWarnings(): invalid parameter");
    return "error";
}
