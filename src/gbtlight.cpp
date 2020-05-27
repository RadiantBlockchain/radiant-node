// Copyright (c) 2020 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <gbtlight.h>
#include <logging.h>
#include <scheduler.h>
#include <util/strencodings.h>
#include <util/system.h>

#include <atomic>
#include <limits>

namespace gbtl {

const char * const DEFAULT_JOB_DATA_SUBDIR = "gbt";
const int DEFAULT_JOB_CACHE_SIZE = 10;
const int64_t DEFAULT_JOB_DATA_EXPIRY_SECS = 3600; // 1 hour
static_assert(DEFAULT_JOB_CACHE_SIZE > 0, "DEFAULT_JOB_CACHE_SIZE must be >0");
const std::string tmpExt = ".tmp";

namespace {
/// Config data based on args cached for performance (since calling gArgs.GetArg() is slow).
/// This should be initialized before RPC server startup via GBTLight::Initialize().
struct Config {
    fs::path storeDir, trashDir;
    int64_t jobDataExpirySecs{};
    int cacheSize{};
};
Config config;
std::atomic_uint initCt;
void CleanJobDataDir();
} // namespace

void Initialize(CScheduler &scheduler) {
    const auto invocationId = ++gbtl::initCt;
    // parse -gbtcachesize
    const auto argCacheSize = gArgs.GetArg("-gbtcachesize", int64_t(DEFAULT_JOB_CACHE_SIZE));
    if (argCacheSize > 0 && argCacheSize <= std::numeric_limits<int>::max()) {
        config.cacheSize = int(argCacheSize);
    } else {
        LogPrintf("WARNING: -gbtcachesize=%d is invalid, forcing setting to %d\n",
                  argCacheSize, DEFAULT_JOB_CACHE_SIZE);
        config.cacheSize = DEFAULT_JOB_CACHE_SIZE;
    }
    // parse -gbtstoredir; init data dir and trash dir, and create them if they don't exist.
    // Note that if this fails here, an exception will be thrown, and the app will exit on init.
    const auto dataDir = GetDataDir(true /* fNetSpecific */);
    const auto storeDirStr = gArgs.GetArg("-gbtstoredir", (dataDir / DEFAULT_JOB_DATA_SUBDIR).string());
    config.storeDir = fs::system_complete(storeDirStr);
    TryCreateDirectories(config.storeDir); // may throw
    config.trashDir = config.storeDir / "trash";
    TryCreateDirectories(config.trashDir); // may throw
    // parse -gbtstoretime
    config.jobDataExpirySecs = gArgs.GetArg("-gbtstoretime", DEFAULT_JOB_DATA_EXPIRY_SECS);
    if (config.jobDataExpirySecs < 0) {
        LogPrintf("WARNING: -gbtstoretime=%d is invalid, forcing setting to %d\n", config.jobDataExpirySecs,
                  DEFAULT_JOB_DATA_EXPIRY_SECS);
        config.jobDataExpirySecs = DEFAULT_JOB_DATA_EXPIRY_SECS;
    }
    // create background task to clean the -gbtstoredir; runs every -gbtstoretime/2 secs
    if (config.jobDataExpirySecs > 0) {
        // schedule cleanup task every config.jobDataExpirySecs/2 seconds
        scheduler.scheduleEvery(
                    // Predicate task (return true -> keep running)
                    [invocationId]{
                        if (gbtl::initCt != invocationId) {
                            // This is a guard against Initialize() being called multiple times -- which may happen in
                            // the test framework.  We detect the situation and return early with a false return,
                            // indicating to the scheduler to not run this lambda instance again.
                            // The newer invocation that supplanted us will now be responsible for cleaning.
                            // Thus, we ensure there is only 1 extant copy of this scheduled task active at any 1
                            // time.
                            return false;
                        }
                        CleanJobDataDir();
                        return true;
                    },
                    // Task interval in milliseconds. This will always be at least 500ms, default is 1,800,000 (30 mins)
                    (config.jobDataExpirySecs * 1000L) / 2L
        );
        // We run the cleanup task once "soon" if this is the first time Initialize() was called, to clean any stale
        // files immediately at startup.
        if (invocationId == 1 && config.jobDataExpirySecs > 2)
            scheduler.scheduleFromNow([]{CleanJobDataDir();}, 100);
    }
}

const fs::path &GetJobDataDir() { return config.storeDir; }
const fs::path &GetJobDataTrashDir() { return config.trashDir; }
size_t GetJobCacheSize() { return size_t(config.cacheSize); }
int64_t GetJobDataExpiry() { return config.jobDataExpirySecs; }

namespace {
void CleanJobDataDir() {
    // this is intended to run as a background task off the scheduler on the order of once every 60 minutes
    // it cleans up the job data dir for the GBTLight subsystem.
    static const std::string pfx{"GBTLCleanJobData"};
    const auto t0 = GetTimeMicros();
    if (config.jobDataExpirySecs <= 0) {
        LogPrintf("WARNING: %s called but expiry time <= 0\n", pfx);
        return;
    }
    unsigned count = 0, total = 0;
    const auto RM = [&count](const fs::path &p) {
        try {
            fs::remove(p);
            ++count;
            LogPrint(BCLog::RPC, "%s: file %s deleted\n", pfx, p.filename().string());
        } catch (const std::exception &e) {
            LogPrintf("WARNING: %s failed to delete file %s: %s\n", pfx, p.string(), e.what());
        }
    };
    const auto MV = [&count](const fs::path &src, const fs::path &dst) {
        try {
            fs::rename(src, dst);
            ++count;
            LogPrint(BCLog::RPC, "%s: file %s moved to %s\n", pfx, src.filename().string(),
                     dst.parent_path().filename().string());
        } catch (const std::exception &e) {
            LogPrintf("WARNING: %s failed to move file %s -> %s: %s\n", pfx, src.string(), dst.parent_path().string(),
                      e.what());
        }
    };
    const int64_t cutoff = GetSystemTimeInSeconds() - config.jobDataExpirySecs,
                  cutoffTrash = cutoff + config.jobDataExpirySecs/2L;
    const auto &jobDir = GetJobDataDir(), &trashDir = GetJobDataTrashDir();
    // process gbt/trash/ dir
    for (const auto &entry : fs::directory_iterator(trashDir)) {
        const auto &path = entry.path();
        if (!fs::is_regular_file(path))
            continue;
        ++total;
        const auto mtime = fs::last_write_time(path);
        if (mtime < cutoff) {
            // too old, delete
            RM(path);
        }
    }
    // process gbt/ data dir
    for (const auto &entry : fs::directory_iterator(jobDir)) {
        const auto &path = entry.path();
        const auto basename = path.filename().stem().string();
        if (!fs::is_regular_file(path) || basename.size() != JobId::size()*2 || !IsHex(basename)) {
            // we only delete/move regular files that have a hex filename (that look like a jobId), otherwise
            // we ignore everything else.
            continue;
        }
        ++total;
        const auto mtime = fs::last_write_time(path);
        if (mtime >= cutoffTrash) {
            // too new, leave it
            continue;
        }
        if (mtime < cutoff || path.extension() == tmpExt) {
            // too old, or is "old-enough" tmp file. Delete unconditionally without moving to trash.
            RM(path);
            continue;
        }
        // newer than absolute cutoff, older than trash cutoff -- move to trash for "purgatory"
        MV(path, trashDir / path.filename());
    }
    if (count) {
        LogPrint(BCLog::RPC, "%s cleaned or moved %u out of %u item(s) in %f secs\n", pfx, count, total,
                 (GetTimeMicros()-t0)/1e6);
    }
}
} // namespace

} // namespace gbtl
