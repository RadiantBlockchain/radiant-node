// Copyright (c) 2018-2019 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <cstdint>

class CBlockIndex;

namespace Consensus {
struct Params;
}

/** Check if UAHF has activated. */
bool IsUAHFenabled(const Consensus::Params &params,
                   const CBlockIndex *pindexPrev);

/** Check if Jul 09 2022 10:00:00 GMT+000 0(1657360800) ASERT DAA has activated. */
bool IsASERTEnabled(const Consensus::Params &params,
                    const CBlockIndex *pindexPrev);