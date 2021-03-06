#pragma once
/**
 * Chain params constants for each tracked chain.
 * @generated by contrib/devtools/chainparams/generate_chainparams_constants.py
 */

#include <primitives/blockhash.h>
#include <uint256.h>

namespace ChainParamsConstants {
    const BlockHash MAINNET_DEFAULT_ASSUME_VALID = BlockHash::fromHex("0000000065d8ed5d8be28d6876b3ffb660ac2a6c0ca59e437e1f7a6f4e003fb4");
    const uint256 MAINNET_MINIMUM_CHAIN_WORK = uint256S("00000000000000000000000000000000000000000000000000114d714d714d60");

    const BlockHash TESTNET_DEFAULT_ASSUME_VALID = BlockHash::fromHex("0000000031a03c315a3758e5611cdd1410e1e0da9dc905769e8db8f6a7bf6d8e");
    const uint256 TESTNET_MINIMUM_CHAIN_WORK = uint256S("00000000000000000000000000000000000000000000000000000e200e200e20");

    const BlockHash TESTNET4_DEFAULT_ASSUME_VALID = BlockHash::fromHex("00000000fb574b8ace948acaa62a60ef24ee504b9f8fbf430ac77f514ea1f6fc");
    const uint256 TESTNET4_MINIMUM_CHAIN_WORK = uint256S("00000000000000000000000000000000000000000000000000000e200e200e20");

    // Scalenet re-organizes above height 10,000 - use block 9,999 hash here.
    const BlockHash SCALENET_DEFAULT_ASSUME_VALID = BlockHash::fromHex("0000000090d0f97e1825d5e8c47ad32110abad0f7340f73e5b3d92baf55b2a76");
    const uint256 SCALENET_MINIMUM_CHAIN_WORK = uint256S("00000000000000000000000000000000000000000000000000000e200e200e20");
} // namespace ChainParamsConstants
