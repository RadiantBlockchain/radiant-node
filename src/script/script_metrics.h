// Copyright (c) 2020 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

/**
 * Struct for holding cumulative results from executing a script or a sequence
 * of scripts.
 */
struct ScriptExecutionMetrics {
    int nSigChecks = 0;
};
