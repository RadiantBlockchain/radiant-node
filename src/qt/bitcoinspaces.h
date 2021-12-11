// Copyright (c) 2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <QChar>

namespace BitcoinSpaces {
// U+2009 THIN SPACE = UTF-8 E2 80 89
inline constexpr QChar realThin = 0x2009;
inline constexpr  auto realThinUtf8 = "\xE2\x80\x89";
inline constexpr  auto realThinHtml = "&thinsp;";

// U+200A HAIR SPACE = UTF-8 E2 80 8A
inline constexpr QChar hair = 0x200A;
inline constexpr  auto hairUtf8 = "\xE2\x80\x8A";
inline constexpr  auto hairHtml = "&#8202;";

// U+2006 SIX-PER-EM SPACE = UTF-8 E2 80 86
inline constexpr QChar sixPerEm = 0x2006;
inline constexpr  auto sixPerEmUtf8 = "\xE2\x80\x86";
inline constexpr  auto sixPerEmHtml = "&#8198;";

// U+2007 FIGURE SPACE = UTF-8 E2 80 87
inline constexpr QChar figure = 0x2007;
inline constexpr  auto figureUtf8 = "\xE2\x80\x87";
inline constexpr  auto figureHtml = "&#8199;";

// QMessageBox seems to have a bug whereby it doesn't display thin/hair spaces
// correctly. Workaround is to display a space in a small font. If you change
// this, please test that it doesn't cause the parent span to start wrapping.
inline constexpr auto htmlHackSpace =
    "<span style='white-space: nowrap; font-size: 6pt'> </span>";

// Define thin* variables to be our preferred type of thin space
inline constexpr auto thin = realThin;
inline constexpr auto thinUtf8 = realThinUtf8;
inline constexpr auto thinHtml = htmlHackSpace;

} // namespace BitcoinSpaces
