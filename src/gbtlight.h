// Copyright (c) 2020 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <fs.h>
#include <uint256.h>

#include <cstdint>
#include <string>

class CScheduler;

/// getblocktemplatelight and submitblocklight related config variables and functions
namespace gbtl {
using JobId = uint160;
/// Called once at app init if -server=true to set up some internal variables and create the subdirectory that the
/// GBTLight subsystem uses.  Also creates a scheduler task for cleaning up old gbt light data files periodically
/// (every hour).
///
/// May throw a std::exception subclass if it fails to create the gbt/ subdirectory.  Calling code should catch this
/// and shutdown the app in that case.
void Initialize(CScheduler &scheduler);

/// Returns the "gbt" directory path. From arg -gbtstoredir=<dir> (default: <datadir>/gbt).
const fs::path &GetJobDataDir();
/// Returns the "gbt trash" directory path. This is always GetJobDataDir() / "trash".
const fs::path &GetJobDataTrashDir();
/// Returns the size of the in-memory jobId cache we should use. From arg -gbtcachesize=<n>
size_t GetJobCacheSize();
/// Returns the job data dir file expiry time in seconds.  From arg -gbtstoretime=<n>
int64_t GetJobDataExpiry();

extern const int DEFAULT_JOB_CACHE_SIZE; /**< = 10 */
extern const char * const DEFAULT_JOB_DATA_SUBDIR; /**< = "gbt" */
extern const int64_t DEFAULT_JOB_DATA_EXPIRY_SECS; /**< = 3600 */
extern const std::string tmpExt; /** = ".tmp" */
}
