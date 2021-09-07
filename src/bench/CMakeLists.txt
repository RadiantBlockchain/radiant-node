# Copyright (c) 2018-2020 The Bitcoin developers

project(bench_bitcoin)

add_executable(bench_bitcoin
	EXCLUDE_FROM_ALL
	banman.cpp
	addrman.cpp
	base58.cpp
	bench.cpp
	bench_bitcoin.cpp
	block_assemble.cpp
	cashaddr.cpp
	ccoins_caching.cpp
	chained_tx.cpp
	checkblock.cpp
	checkqueue.cpp
	crypto_aes.cpp
	crypto_hash.cpp
	data/block413567.cpp
	data/block556034.cpp
	data/coins_spent_413567.cpp
	data/coins_spent_556034.cpp
	dsproof.cpp
	duplicate_inputs.cpp
	examples.cpp
	gcs_filter.cpp
	lockedpool.cpp
	mempool_eviction.cpp
	merkle_root.cpp
	prevector.cpp
	removeforblock.cpp
	rollingbloom.cpp
	rpc_blockchain.cpp
	rpc_mempool.cpp
	json.cpp
	util_time.cpp
	verify_script.cpp

	# TODO: make a test library
	../test/setup_common.cpp
	../test/util.cpp
)

target_link_libraries(bench_bitcoin common bitcoinconsensus server)

if(BUILD_BITCOIN_WALLET)
	target_sources(bench_bitcoin PRIVATE coin_selection.cpp)
	target_link_libraries(bench_bitcoin wallet)
endif()

include(InstallationHelper)
install_target(bench_bitcoin EXCLUDE_FROM_ALL)

add_custom_target(bench-bitcoin COMMAND bench_bitcoin USES_TERMINAL)
add_custom_target(bitcoin-bench DEPENDS bench_bitcoin)
