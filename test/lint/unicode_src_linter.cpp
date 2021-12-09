// Copyright (c) 2021 Calin A. Culianu <calin.culianu@gmail.com>
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/*
Unicode Linter

This is a simple CLI program that takes a single argument, a filename. It then
proceeds to check that the file does not contain any "unsafe" unicode
codepoints, and if it does, it exits with status 1. If it does not, it exits
with status 0. If it does detect some unsafe codepoints, it will print some
errors to stderr.

"Unsafe" unicode codepoints can lead to a difference between how a file is read
by a human and how it is interpreted by a compiler. Codepoints that silently
move around text, for example, can be used to obfuscate or mislead humans that
are reviewing sourcecode. This can be used to exploit open source projects by
getting code added that behaves differently than how it would appear from
reading the sourcecode.

See: https://nvd.nist.gov/vuln/detail/CVE-2021-42574
*/


#include <algorithm>
#include <array>
#include <cerrno>
#include <cinttypes>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <optional>
#include <string>
#include <utility>
#include <unordered_set>
#include <vector>

#if __has_include(<unistd.h>)
#define IS_UNIX 1
#include <unistd.h>
#else
#define IS_UNIX 0
#endif

namespace {

template <typename F>
struct Defer {
    F func;
    Defer(F && f) : func(std::move(f)) {}
    ~Defer() { func(); }
};

using CodePoint = uint32_t;
using CodePointSet = std::unordered_set<CodePoint>;

struct Options {
    bool ignoreErrors = false;
    CodePointSet suppressions;
};

/**
 * Filter that generates and validates UTF-8, as well as collates UTF-16
 * surrogate pairs as specified in RFC4627.
 */
struct UTF8StringProc
{
    int state = 0;
    CodePoint codepoint = 0; // the currently active midstate codepoint, or last full codepoint
    std::vector<CodePoint> recent_multi;

    enum Status { OK, Midstate, Nonminimal, Error };

    UTF8StringProc() { recent_multi.reserve(8); }

    // Write single 8-bit char (may be part of UTF-8 sequence)
    Status update(unsigned char ch)
    {
        if (state == 0) {
            if (ch < 0x80) { // 7-bit ASCII, fast direct pass-through
                if (surpair) return Error;
                recent_multi.clear();
                return OK;
            } else if (ch < 0xc0) // Mid-sequence character, invalid in this state
                return Error;
            else if (ch < 0xe0) { // Start of 2-byte sequence
                codepoint = (ch & 0x1f) << 6;
                state = 6;
                return Midstate;
            } else if (ch < 0xf0) { // Start of 3-byte sequence
                codepoint = (ch & 0x0f) << 12;
                state = 12;
                return Midstate;
            } else if (ch < 0xf8) { // Start of 4-byte sequence
                codepoint = (ch & 0x07) << 18;
                state = 18;
                return Midstate;
            } else // Reserved, invalid
                return Error;
        } else {
            if ((ch & 0xc0) != 0x80) // Not a continuation, invalid
                return Error;
            state -= 6;
            codepoint |= (ch & 0x3f) << state;
            if (state == 0) {
                return accept_codepoint(codepoint);
            }
            return Midstate;
        }
    }

private:
    // Keep track of the following state to handle the following section of
    // RFC4627:
    //
    //    To escape an extended character that is not in the Basic Multilingual
    //    Plane, the character is represented as a twelve-character sequence,
    //    encoding the UTF-16 surrogate pair.  So, for example, a string
    //    containing only the G clef character (U+1D11E) may be represented as
    //    "\uD834\uDD1E".
    //
    //  Two subsequent \u.... may have to be replaced with one actual codepoint.
    CodePoint surpair = 0; // First half of open UTF-16 surrogate pair, or 0

    Status append_codepoint(CodePoint cp)
    {
        if (cp <= 0x7F) {
            recent_multi.clear();
            return Nonminimal;
        } else if (cp <= 0x1FFFFF) {
            recent_multi.push_back(cp);
            return OK;
        }
        return Error;
    }

    Status accept_codepoint(CodePoint cp)
    {
        if (state) // Only accept full codepoints in open state
            return Error;
        if (cp >= 0xD800 && cp < 0xDC00) { // First half of surrogate pair
            if (surpair) // Two subsequent surrogate pair openers - fail
                return Error;
            else {
                surpair = cp;
                return Midstate;
            }
        } else if (cp >= 0xDC00 && cp < 0xE000) { // Second half of surrogate pair
            if (surpair) { // Open surrogate pair, expect second half
                // Compute code point from UTF-16 surrogate pair
                append_codepoint(0x10000 | ((surpair - 0xD800)<<10) | (cp - 0xDC00));
                surpair = 0;
                return OK;
            } else // Second half doesn't follow a first half - fail
                return Error;
        } else {
            if (surpair) // First half of surrogate pair not followed by second - fail
                return Error;
            return append_codepoint(cp);
        }

    }
};


bool isForbiddenCodepoint(const Options &options, CodePoint cp)
{
    static const CodePointSet bidiControlChars{{
        // bidi chars
        0x202a, 0x202b, 0x202c, 0x202d, 0x202e, 0x2066, 0x2067, 0x2068, 0x2069,
    }};
    static const CodePointSet controlChars{{
        // non-printable control characters in unicode space, excluding bidi
        0x110bd, 0x110cd, 0x13430, 0x13431, 0x13432, 0x13433, 0x13434, 0x13435, 0x13436, 0x13437, 0x13438, 0x180e,
        0x1bca0, 0x1bca1, 0x1bca2, 0x1bca3, 0x1d173, 0x1d174, 0x1d175, 0x1d176, 0x1d177, 0x1d178, 0x1d179, 0x1d17a,
        0x200b, 0x200c, 0x200d, 0x200e, 0x200f, 0x202a, 0x202b, 0x202c, 0x202d, 0x202e, 0x2060, 0x2061, 0x2062, 0x2063,
        0x2064, 0x2066, 0x2067, 0x2068, 0x2069, 0x206a, 0x206b, 0x206c, 0x206d, 0x206e, 0x206f, 0x600, 0x601, 0x602,
        0x603, 0x604, 0x605, 0x61c, 0x6dd, 0x70f, 0x8e2, 0xad, 0xe0001, 0xe0020, 0xe0021, 0xe0022, 0xe0023, 0xe0024,
        0xe0025, 0xe0026, 0xe0027, 0xe0028, 0xe0029, 0xe002a, 0xe002b, 0xe002c, 0xe002d, 0xe002e, 0xe002f, 0xe0030,
        0xe0031, 0xe0032, 0xe0033, 0xe0034, 0xe0035, 0xe0036, 0xe0037, 0xe0038, 0xe0039, 0xe003a, 0xe003b, 0xe003c,
        0xe003d, 0xe003e, 0xe003f, 0xe0040, 0xe0041, 0xe0042, 0xe0043, 0xe0044, 0xe0045, 0xe0046, 0xe0047, 0xe0048,
        0xe0049, 0xe004a, 0xe004b, 0xe004c, 0xe004d, 0xe004e, 0xe004f, 0xe0050, 0xe0051, 0xe0052, 0xe0053, 0xe0054,
        0xe0055, 0xe0056, 0xe0057, 0xe0058, 0xe0059, 0xe005a, 0xe005b, 0xe005c, 0xe005d, 0xe005e, 0xe005f, 0xe0060,
        0xe0061, 0xe0062, 0xe0063, 0xe0064, 0xe0065, 0xe0066, 0xe0067, 0xe0068, 0xe0069, 0xe006a, 0xe006b, 0xe006c,
        0xe006d, 0xe006e, 0xe006f, 0xe0070, 0xe0071, 0xe0072, 0xe0073, 0xe0074, 0xe0075, 0xe0076, 0xe0077, 0xe0078,
        0xe0079, 0xe007a, 0xe007b, 0xe007c, 0xe007d, 0xe007e, 0xe007f, 0xfeff, 0xfff9, 0xfffa, 0xfffb,
    }};
    static const CodePointSet homoglyphs{{
        // characters from other alphabets that *may* look like latin1 characters in some font renderings
        0x395, 0x421, 0x4c0, 0x4cf, 0x1e37, 0x458, 0x1e3b, 0x1e42, 0x1ea0, 0x1e3d, 0x445, 0x41e, 0x2cb, 0x1e2c, 0x39f,
        0x1e11, 0x1e24, 0x1e05, 0x13b, 0x3bf, 0x13c, 0x3f2, 0x1d71, 0x1ef5, 0x219, 0x1e96, 0x1e49, 0x1e89, 0x6c, 0x396,
        0x1e92, 0x122, 0x1e2b, 0x1e94, 0x26d, 0x1e7e, 0x1e77, 0x1e5e, 0x1e36, 0x39c, 0x3f9, 0x1e71, 0x3a7, 0x1fbc,
        0x1e88, 0x1e19, 0x1ecd, 0x1e34, 0x1ee4, 0x41d, 0x1e07, 0x1e76, 0x2c3, 0x1e01, 0x1e63, 0x71, 0x1e48, 0x1eb9,
        0x1e46, 0x1e2a, 0x1e5a, 0x425, 0x1e74, 0x3a1, 0x391, 0x1e43, 0x440, 0x24b, 0x12e, 0x1e4b, 0x41c, 0x443, 0x137,
        0x1eb8, 0x1d21, 0x415, 0x1d22, 0x1e75, 0x422, 0x1e2d, 0x21b, 0x1d20, 0x1e3c, 0x1fef, 0x1ecb, 0x1ee5, 0x397,
        0x1e62, 0x1e0e, 0x1e04, 0x410, 0x1ecc, 0x1e0d, 0x1e35, 0x1d04, 0x405, 0x1e5b, 0x1c0, 0x406, 0x146, 0x3a4,
        0x1e6f, 0x43e, 0x157, 0x145, 0x1e18, 0x261, 0x1d0f, 0x1e25, 0x1e6c, 0x456, 0x420, 0x1e10, 0x1eca, 0x1e5f,
        0x1e6d, 0x1fcc, 0x1e47, 0x455, 0x1ea1, 0x1e70, 0x1e13, 0x49, 0x1e4a, 0x1e0c, 0x2c2, 0xad, 0x1e6e, 0x67, 0x1e0f,
        0x1e1b, 0x1e12, 0x39a, 0x1e73, 0x1e06, 0x501, 0x39d, 0x21a, 0x435, 0x441, 0x4bb, 0x408, 0x1c3, 0x1e32, 0x1e95,
        0x156, 0x3f3, 0x392, 0x218, 0x1ef4, 0x196, 0x2ba, 0x2b9, 0x1e33, 0x1e3a, 0x1e7f, 0x1e00, 0x1e93, 0x430, 0x412,
        0x3a5, 0x3bd, 0x399, 0x136, 0x1e72, 0x37e, 0x1e1a,
    }};

    if (options.suppressions.find(cp) != options.suppressions.end())
        return false; // appears in ALLOW= from env, so allow this cp unconditionally

    return bidiControlChars.find(cp) != bidiControlChars.end()
            || controlChars.find(cp) != controlChars.end()
            || homoglyphs.find(cp) != homoglyphs.end();
}

bool containsForbiddenCodepoints(const Options &options, const UTF8StringProc &proc)
{
    for (const CodePoint cp : proc.recent_multi)
        if (isForbiddenCodepoint(options, cp)) return true;
    return false;
}

bool checkFile(const Options &options, const char *file)
{
    FILE *f = std::fopen(file, "r");
    if (!f) {
        std::fprintf(stderr, "Failed to open \"%s\": %s\n", file, std::strerror(errno));
        return false;
    }
    Defer closer([&f]{ if (f) std::fclose(f), f = nullptr; });

    std::array<unsigned char, 16384> buf;
    int nread = 0, i = 0;
    size_t total = 0;
    UTF8StringProc proc;
    auto lastStatus = proc.OK;
    auto showError = [&] {
        // non-ascii (high bit set), print (possibly colored) error to stderr
        const char *RED = "", *BRIGHT = "", *CYAN = "", *NORMAL = "";
#if IS_UNIX
        if (isatty(fileno(stderr))) {
            RED = "\x1b[31;1m";
            BRIGHT = "\x1b[37;1m";
            CYAN = "\x1b[36m";
            NORMAL = "\x1b[0m";
        }
#endif
        constexpr int context = 40;
        auto start = buf.cbegin() + (i >= context / 2 ? i - context / 2 : 0);
        auto end = start + nread;
        end = start + context > end ? end : start + context;
        std::string contextStr{reinterpret_cast<const char *>(&*start),
                               reinterpret_cast<const char *>(&*end)};
        std::string statusMsg, forbiddenStr;
        switch (lastStatus) {
        case proc.Error: statusMsg = "Unicode error"; break;
        case proc.Nonminimal: statusMsg = "Non-minimally encoded codepoint(s)"; break;
        case proc.Midstate: statusMsg = "Unterminated unicode sequence"; break;
        default: statusMsg = "Forbidden codepoint(s)"; break;
        }

        std::vector<CodePoint> forbidden;
        for (auto cp : proc.recent_multi) if (isForbiddenCodepoint(options, cp)) forbidden.push_back(cp);
        if (!forbidden.empty()) {
            statusMsg += " {";
            int j = 0;
            for (auto cp : forbidden) {
                char buf[256];
                std::snprintf(buf, 256, "%s%s0x%x%s", j++ ? ", " : "", BRIGHT, unsigned(cp), CYAN);
                statusMsg += buf;
            }
            statusMsg += "}";
        }

        std::fprintf(stderr, "%s%s in file \"%s%s%s\","
                             " at byte pos %s%lu%s, context:\n%s\"%s%s%s\"%s\n",
                     CYAN, statusMsg.c_str(),
                     BRIGHT, file, CYAN,
                     BRIGHT, static_cast<unsigned long>(total + i), CYAN,
                     RED, NORMAL, contextStr.c_str(), RED, NORMAL);
    };

    while ((nread = std::fread(buf.data(), sizeof(buf[0]), buf.size(), f)) > 0) {
        for (i = 0; i < nread; ++i) {
            lastStatus = proc.update(buf[i]);
            if ((lastStatus == proc.Error && !options.ignoreErrors)
                || (lastStatus == proc.Nonminimal && !options.ignoreErrors)
                || containsForbiddenCodepoints(options, proc)) {
                showError();
                return false;
            }
            if (lastStatus == proc.OK)
                proc.recent_multi.clear(); // clear recent good codepoints to avoid re-scanning them in the future
        }
        total += nread;
    }
    if (!options.ignoreErrors && lastStatus != proc.OK) {
        showError();
        return false;
    }

    return true;
}

std::vector<std::string> split(const std::string &s, const std::string &delims = ",", bool stripWhitespace = true)
{
    std::vector<std::string> ret;
    size_t l = 0, r;
    do {
        r = s.find_first_of(delims, l);
        if (r > s.size()) r = s.size();
        auto tok = s.substr(l, r - l);
        if (stripWhitespace)
            tok.erase(std::remove_if(tok.begin(), tok.end(), [](int ch){ return std::isspace(ch); }), tok.end());
        ret.push_back(std::move(tok));
        l = r + 1;
    } while (l < s.size());
    return ret;
}

// Parses ALLOW= and DECODE_ERRORS= and puts them in `options`
std::optional<Options> parseEnv()
{
    Options options;
    if (const char *e = std::getenv("DECODE_ERRORS")) {
        uint8_t yesno;
        if (std::sscanf(e, "%" PRIu8, &yesno) != 1) {
            std::fprintf(stderr, "Error parsing DECODE_ERRORS env var. '%s'. Not a number.\n\n", e);
            return std::nullopt;
        }
        options.ignoreErrors = !yesno;
    }
    if (const char *e = std::getenv("ALLOW")) {
        const auto toks = split(e);
        for (const auto & tok : toks) {
            CodePoint cp;
            if (std::sscanf(tok.c_str(), "%" PRIx32, &cp) != 1) {
                std::fprintf(stderr, "Error parsing ALLOW env var. '%s' is not a hex number.\n\n", tok.c_str());
                return std::nullopt;
            }
            options.suppressions.insert(cp);
        }
    }
    return options;
}

} // namespace

int main(int argc, const char *argv[])
{
    std::optional<Options> options;
    if (argc != 2 || !(options = parseEnv())) {
        std::fprintf(stderr,
                     "Usage: %s file\n\n"
                     "Influential environment variables:\n\n"
                     "ALLOW=<hex>[,<hex>...]     A comma-delimited list of hex values which will be\n"
                     "                           interpreted by this progream as Unicode codepoints\n"
                     "                           that should be allowed, even if they appear in the\n"
                     "                           program's internal exclusion lists.\n\n"
                     "DECODE_ERRORS=<num>        If non-zero, fail on Unicode decode errors. If 0,\n"
                     "                           then allow Unicode decode errors. Default: 1.\n",
                     argv[0]);
        return EXIT_FAILURE;
    }
    return checkFile(*options, argv[1]) ? EXIT_SUCCESS : EXIT_FAILURE;
}
