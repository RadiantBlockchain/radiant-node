// Copyright (c) 2017 The Bitcoin Core developers
// Copyright (c) 2019 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <fs.h>

#ifndef WIN32
#include <fcntl.h>
#include <string>
#include <sys/file.h>
#include <sys/utsname.h>
#else
#include <codecvt>
#include <windows.h>
#endif

namespace fsbridge {

FILE *fopen(const fs::path &p, const char *mode) {
    return ::fopen(p.string().c_str(), mode);
}

#ifndef WIN32

static std::string GetErrorReason() {
    return std::strerror(errno);
}

FileLock::FileLock(const fs::path &file) {
    fd = open(file.string().c_str(), O_RDWR);
    if (fd == -1) {
        reason = GetErrorReason();
    }
}

FileLock::~FileLock() {
    if (fd != -1) {
        close(fd);
    }
}

// Returns true if and only if this system is WSL v1. v2, Linux, macOS, etc always return false.
static bool IsWSLv1() {
    struct utsname uname_data;
    return uname(&uname_data) == 0 &&
           std::string(uname_data.version).find("Microsoft") !=
               std::string::npos;
}

bool FileLock::TryLock() {
    if (fd == -1) {
        return false;
    }

    // Exclusive file locking is broken on WSL v1 only using fcntl (Core issue #18622).
    // This workaround can be removed once the bug on WSLv1 is fixed.
    // WSL v2 does not suffer from this bug.
    static const bool is_wsl_v1 = IsWSLv1();
    if (is_wsl_v1) {
        if (flock(fd, LOCK_EX | LOCK_NB) == -1) {
            reason = GetErrorReason();
            return false;
        }
    } else {
        struct flock lock;
        lock.l_type = F_WRLCK;
        lock.l_whence = SEEK_SET;
        lock.l_start = 0;
        lock.l_len = 0;
        if (fcntl(fd, F_SETLK, &lock) == -1) {
            reason = GetErrorReason();
            return false;
        }
    }

    return true;
}
#else

static std::string GetErrorReason() {
    wchar_t *err;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                       FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, GetLastError(),
                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                   reinterpret_cast<WCHAR *>(&err), 0, nullptr);
    std::wstring err_str(err);
    LocalFree(err);
    return std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().to_bytes(
        err_str);
}

FileLock::FileLock(const fs::path &file) {
    hFile = CreateFileW(file.wstring().c_str(), GENERIC_READ | GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        reason = GetErrorReason();
    }
}

FileLock::~FileLock() {
    if (hFile != INVALID_HANDLE_VALUE) {
        CloseHandle(hFile);
    }
}

bool FileLock::TryLock() {
    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }
    _OVERLAPPED overlapped = {0};
    if (!LockFileEx(hFile, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY,
                    0, 0, 0, &overlapped)) {
        reason = GetErrorReason();
        return false;
    }
    return true;
}
#endif

std::string get_filesystem_error_message(const fs::filesystem_error &e) {
#ifndef WIN32
    return e.what();
#else
    // Convert from Multi Byte to utf-16
    std::string mb_string(e.what());
    int size = MultiByteToWideChar(CP_ACP, 0, mb_string.c_str(),
                                   mb_string.size(), nullptr, 0);

    std::wstring utf16_string(size, L'\0');
    MultiByteToWideChar(CP_ACP, 0, mb_string.c_str(), mb_string.size(),
                        &*utf16_string.begin(), size);
    // Convert from utf-16 to utf-8
    return std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t>()
        .to_bytes(utf16_string);
#endif
}

} // namespace fsbridge
