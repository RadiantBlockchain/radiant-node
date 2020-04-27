# Copyright (c) 2018-2020 The Bitcoin developers

project(bench_bitcoin)

add_executable(bench_bitcoin
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
	util_time.cpp

	# TODO: make a test library
	../test/setup_common.cpp
)

target_link_libraries(bench_bitcoin common bitcoinconsensus server)

if(BUILD_BITCOIN_WALLET)
	target_sources(bench_bitcoin PRIVATE coin_selection.cpp)
	target_link_libraries(bench_bitcoin wallet)
endif()

add_custom_target(bench-bitcoin COMMAND bench_bitcoin USES_TERMINAL)
add_custom_target(bitcoin-bench DEPENDS bench_bitcoin)
