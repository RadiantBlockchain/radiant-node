# Copyright (c) 2018 The Bitcoin developers

project(bitcoin-test)

option(ENABLE_PROPERTY_BASED_TESTS "Enable property based tests" OFF)

# Process json files.
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/data")

function(gen_json_header NAME)
  set(HEADERS "")
  foreach(f ${ARGN})
    set(h "${CMAKE_CURRENT_BINARY_DIR}/${f}.h")

    # Get the proper name for the test variable.
    get_filename_component(TEST_NAME ${f} NAME_WE)
    add_custom_command(OUTPUT ${h}
      COMMAND
        "${Python_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/data/generate_header.py"
        "${TEST_NAME}"
        "${CMAKE_CURRENT_SOURCE_DIR}/${f}" > ${h}
      MAIN_DEPENDENCY ${f}
      DEPENDS
        "data/generate_header.py"
      VERBATIM
    )
    list(APPEND HEADERS ${h})
  endforeach(f)
  set(${NAME} "${HEADERS}" PARENT_SCOPE)
endfunction()

gen_json_header(JSON_HEADERS
  data/base58_encode_decode.json
  data/blockfilters.json
  data/key_io_valid.json
  data/key_io_invalid.json
  data/script_tests.json
  data/sighash.json
  data/tx_invalid.json
  data/tx_valid.json
)

include(TestSuite)
create_test_suite(bitcoin)
add_dependencies(check check-bitcoin)

if(BUILD_RADIANT_WALLET)
  set(BITCOIN_WALLET_TEST_FIXTURE
    ../wallet/test/init_test_fixture.cpp
    ../wallet/test/wallet_test_fixture.cpp
  )
  set(BITCOIN_WALLET_TESTS
    ../wallet/test/db_tests.cpp
    ../wallet/test/init_tests.cpp
    ../wallet/test/psbt_wallet_tests.cpp
    ../wallet/test/wallet_tests.cpp
    ../wallet/test/walletdb_tests.cpp
    ../wallet/test/wallet_crypto_tests.cpp
  )
endif()

if(ENABLE_PROPERTY_BASED_TESTS)
  find_package(Rapidcheck REQUIRED)

  set(BITCOIN_PROPERTY_BASED_TEST_FIXTURE
    gen/crypto_gen.cpp
  )
  set(BITCOIN_PROPERTY_BASED_TESTS
    key_properties.cpp
  )
endif()

function(gen_asmap_headers HEADERS_VAR)
  foreach(INPUT_FILE ${ARGN})
    set(OUTPUT_FILE "${CMAKE_CURRENT_BINARY_DIR}/${INPUT_FILE}.h")

    add_custom_command(
      OUTPUT "${OUTPUT_FILE}"
      COMMENT "Generate ASMAP header from ${INPUT_FILE}"
      COMMAND
        "${Python_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/data/generate_asmap.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/${INPUT_FILE}"
        "${OUTPUT_FILE}"
      MAIN_DEPENDENCY "${INPUT_FILE}"
      DEPENDS
        "data/generate_header.py"
      VERBATIM
    )
    list(APPEND ${HEADERS_VAR} "${OUTPUT_FILE}")
  endforeach()
  set(${HEADERS_VAR} ${${HEADERS_VAR}} PARENT_SCOPE)
endfunction()

gen_asmap_headers(ASMAP_HEADERS
  data/asmap.raw
)

add_boost_unit_tests_to_suite(bitcoin test_bitcoin
  check_assert.cpp
  jsonutil.cpp
  lib/transaction_utils.cpp
  main.cpp
  scriptflags.cpp
  sigutil.cpp
  setup_common.cpp

  ${ASMAP_HEADERS}

  # Tests generated from JSON
  ${JSON_HEADERS}

  # Wallet test fixture
  ${BITCOIN_WALLET_TEST_FIXTURE}

  # Properties based test fixture
  ${BITCOIN_PROPERTY_BASED_TEST_FIXTURE}

  TESTS
    activation_tests.cpp
    addrman_tests.cpp
    allocator_tests.cpp
    amount_tests.cpp
    arith_uint256_tests.cpp
    base32_tests.cpp
    base58_tests.cpp
    base64_tests.cpp
    bip32_tests.cpp
    bitmanip_tests.cpp
    blockchain_tests.cpp
    blockcheck_tests.cpp
    blockencodings_tests.cpp
    blockfilter_tests.cpp
    blockindex_tests.cpp
    blockstatus_tests.cpp
    bloom_tests.cpp
    bswap_tests.cpp
    cashaddrenc_tests.cpp
    cashaddr_tests.cpp
    checkdatasig_tests.cpp
    checkpoints_tests.cpp
    checkqueue_tests.cpp
    coins_tests.cpp
    compress_tests.cpp
    config_tests.cpp
    core_io_tests.cpp
    crypto_tests.cpp
    cuckoocache_tests.cpp
    dbwrapper_tests.cpp
    deadlock_tests.cpp
    denialofservice_tests.cpp
    descriptor_tests.cpp
    dsproof_dspidptr_tests.cpp
    dsproof_tests.cpp
    dstencode_tests.cpp
    excessiveblock_tests.cpp
    extversion_tests.cpp
    feerate_tests.cpp
    finalization_header_tests.cpp
    finalization_tests.cpp
    flatfile_tests.cpp
    gbtlight_tests.cpp
    getarg_tests.cpp
    hash_tests.cpp
    inv_tests.cpp
    key_io_tests.cpp
    key_tests.cpp
    lcg_tests.cpp
    limitedmap_tests.cpp
    mempool_tests.cpp
    merkleblock_tests.cpp
    merkle_tests.cpp
    miner_tests.cpp
    monolith_opcodes_tests.cpp
    multisig_tests.cpp
    native_introspection_tests.cpp
    netbase_tests.cpp
    net_tests.cpp
    pmt_tests.cpp
    policyestimator_tests.cpp
    pow_tests.cpp
    prevector_tests.cpp
    raii_event_tests.cpp
    random_tests.cpp
    reverselock_tests.cpp
    rpc_server_tests.cpp
    rpc_tests.cpp
    rwcollection_tests.cpp
    sanity_tests.cpp
    scheduler_tests.cpp
    schnorr_tests.cpp
    script_bitfield_tests.cpp
    script_commitment_tests.cpp
    scriptnum_tests.cpp
    script_standard_tests.cpp
    script_tests.cpp
    seedspec6_tests.cpp
    serialize_tests.cpp
    sigcache_tests.cpp
    sigcheckcount_tests.cpp
    sigencoding_tests.cpp
    sighash_tests.cpp
    sighashtype_tests.cpp
    skiplist_tests.cpp
    streams_tests.cpp
    sync_tests.cpp
    testlib_tests.cpp
    timedata_tests.cpp
    torcontrol_tests.cpp
    txindex_tests.cpp
    txvalidationcache_tests.cpp
    txvalidation_tests.cpp
    uint256_tests.cpp
    undo_tests.cpp
    util_tests.cpp
    util_threadnames_tests.cpp
    validation_block_tests.cpp
    validation_tests.cpp
    work_comparator_tests.cpp

    # RPC Tests
    ../rpc/test/server_tests.cpp

    # Wallet tests
    ${BITCOIN_WALLET_TESTS}

    # Properties based tests
    ${BITCOIN_PROPERTY_BASED_TESTS}
)

if(ENABLE_PROPERTY_BASED_TESTS)
  target_link_libraries(test_bitcoin Rapidcheck::rapidcheck)
endif()

function(add_boost_test_runners_with_upgrade_activated SUITE EXECUTABLE)
  set(SUITE_UPGRADE_ACTIVATED "${SUITE}-upgrade-activated")
  get_target_from_suite(${SUITE_UPGRADE_ACTIVATED} TARGET_UPGRADE_ACTIVATED)

  if(NOT TARGET ${TARGET_UPGRADE_ACTIVATED})
    create_test_suite_with_parent_targets(
      ${SUITE_UPGRADE_ACTIVATED}
      check-upgrade-activated
      check-upgrade-activated-extended
    )
    add_dependencies(${TARGET_UPGRADE_ACTIVATED} ${EXECUTABLE})
  endif()

  get_target_from_suite(${SUITE} SUITE_TARGET)
  get_target_property(BOOST_TESTS ${SUITE_TARGET} UNIT_TESTS)

endfunction()

add_boost_test_runners_with_upgrade_activated(bitcoin test_bitcoin)

target_link_libraries(test_bitcoin rpcclient server)
if(TARGET bitcoinconsensus-shared)
  target_link_libraries(test_bitcoin bitcoinconsensus-shared)
else()
  target_link_libraries(test_bitcoin bitcoinconsensus)
endif()

add_subdirectory(fuzz)
