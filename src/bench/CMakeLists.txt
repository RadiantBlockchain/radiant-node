# Copyright (c) 2018-2020 The Bitcoin developers

project(bitcoin-bench)

add_executable(bitcoin-bench
	EXCLUDE_FROM_ALL
	base58.cpp
	bench.cpp
	bench_bitcoin.cpp
	cashaddr.cpp
	ccoins_caching.cpp
	checkblock.cpp
	checkqueue.cpp
	crypto_aes.cpp
	crypto_hash.cpp
	data/block413567.cpp
	duplicate_inputs.cpp
	examples.cpp
	gcs_filter.cpp
	lockedpool.cpp
	mempool_eviction.cpp
	merkle_root.cpp
	prevector.cpp
	rollingbloom.cpp
	rpc_blockchain.cpp
	rpc_mempool.cpp
)

target_link_libraries(bitcoin-bench common bitcoinconsensus wallet server)

if(BUILD_BITCOIN_WALLET)
	target_sources(bitcoin-bench PRIVATE coin_selection.cpp)
endif()

add_custom_target(bench-bitcoin COMMAND bitcoin-bench USES_TERMINAL)
