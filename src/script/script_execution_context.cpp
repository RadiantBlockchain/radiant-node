// Copyright (c) 2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <script/script_execution_context.h>

#include <psbt.h>

#include <cassert>


ScriptExecutionContext::ScriptExecutionContext(unsigned input, const CCoinsViewCache &coinsCache,
                                               CTransactionView tx)
    : nIn(input)
{
    assert(input < tx.vin().size());
    std::vector<Coin> coins;
    coins.reserve(tx.vin().size());
    for (const auto & txin : tx.vin()) {
        const Coin & c = coinsCache.AccessCoin(txin.prevout);
        coins.push_back(c);
    }
    shared = std::make_shared<Shared>(std::move(coins), tx);
}

ScriptExecutionContext::ScriptExecutionContext(unsigned input, const std::vector<PSBTInput> &psbtInputs,
                                               CTransactionView tx)
    : nIn(input)
{
    assert(input < tx.vin().size());
    std::vector<Coin> coins;
    coins.reserve(tx.vin().size());
    for (size_t i = 0; i < tx.vin().size(); ++i) {
        auto &psbti = psbtInputs.at(i);
        coins.emplace_back(psbti.utxo, 1 /* height ignored */, false /* isCoinbase ignored */);
    }
    shared = std::make_shared<Shared>(std::move(coins), tx);
}

ScriptExecutionContext::ScriptExecutionContext(unsigned input, const ScriptExecutionContext &sharedContext)
    : nIn(input), shared(sharedContext.shared)
{
    assert(shared);
}


ScriptExecutionContext::ScriptExecutionContext(unsigned input, const CScript &scriptPubKey, Amount amount,
                                               CTransactionView tx, uint32_t nHeight, bool isCoinbase)
    : nIn(input), limited(true)
{
    assert(input < tx.vin().size());
    std::vector<Coin> coins(tx.vin().size());
    coins[input] = Coin(CTxOut(amount, scriptPubKey), nHeight, isCoinbase);
    shared = std::make_shared<Shared>(std::move(coins), tx);
}

/* static */
std::vector<ScriptExecutionContext>
ScriptExecutionContext::createForAllInputs(CTransactionView tx, const CCoinsViewCache &coinsCache)
{
    std::vector<ScriptExecutionContext> ret;
    ret.reserve(tx.vin().size());
    for (size_t i = 0; i < tx.vin().size(); ++i) {
        if (i == 0) {
            ret.push_back(ScriptExecutionContext(i, coinsCache, tx)); // private c'tor, must use push_back
        } else {
            ret.push_back(ScriptExecutionContext(i, ret.front())); // private c'tor, must use push_back
        }
    }
    return ret;
}

/* static */
std::vector<ScriptExecutionContext>
ScriptExecutionContext::createForAllInputs(CTransactionView tx, const std::vector<PSBTInput> &inputs)
{
    std::vector<ScriptExecutionContext> ret;
    ret.reserve(tx.vin().size());
    for (size_t i = 0; i < tx.vin().size(); ++i) {
        if (i == 0) {
            ret.push_back(ScriptExecutionContext(i, inputs, tx)); // private c'tor, must use push_back
        } else {
            ret.push_back(ScriptExecutionContext(i, ret.front())); // private c'tor, must use push_back
        }
    }
    return ret;
}
