# Copyright (c) 2019-2021 The Bitcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

configure_file(Doxyfile.cmake.in Doxyfile ESCAPE_QUOTES)

option(DOC_ONLINE "Adapt Markdown/HTML documentation for online publication" OFF)
configure_file(
	../cmake/utils/gen-doc-md.sh.in
	gen-doc-md.sh
	@ONLY
)
add_custom_target(doc-md
	COMMENT "Building Markdown documentation..."
	DEPENDS bitcoind
	DEPENDS bitcoin-qt
	DEPENDS bitcoin-cli
	DEPENDS bitcoin-tx
	DEPENDS bitcoin-seeder
	DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/gen-doc-md.sh"
	COMMAND "${CMAKE_CURRENT_BINARY_DIR}/gen-doc-md.sh"
)
add_custom_target(doc-html
	COMMENT "Building HTML documentation..."
	DEPENDS doc-md
	WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
	COMMAND mkdocs build
)

option(ENABLE_MAN "Enable manpages" ON)
if(ENABLE_MAN)
	configure_file(
		../cmake/utils/gen-doc-man-footer.sh.in
		man/gen-doc-man-footer.sh
		@ONLY
	)
	add_custom_command(
		OUTPUT man/footer.h2m
		DEPENDS bitcoind
		DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/man/gen-doc-man-footer.sh"
		WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/man"
		COMMAND ./gen-doc-man-footer.sh
	)
	configure_file(
		../cmake/utils/gen-doc-man.sh.in
		man/gen-doc-man.sh
		@ONLY
	)
	include(GNUInstallDirs)
	set(MAN_DESTINATION "${CMAKE_INSTALL_MANDIR}/man1")
	add_custom_command(
		OUTPUT man/bitcoind.1
		DEPENDS bitcoind
		DEPENDS man/footer.h2m
		DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/man/gen-doc-man.sh"
		WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/man"
		COMMAND ./gen-doc-man.sh bitcoind
	)
	add_custom_target(doc-man-bitcoind
		ALL
		DEPENDS man/bitcoind.1
	)
	install(
		FILES "${CMAKE_CURRENT_BINARY_DIR}/man/bitcoind.1"
		DESTINATION "${MAN_DESTINATION}"
		COMPONENT bitcoind
	)
	if(BUILD_BITCOIN_QT)
		add_custom_command(
			OUTPUT man/bitcoin-qt.1
			DEPENDS bitcoin-qt
			DEPENDS man/footer.h2m
			DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/man/gen-doc-man.sh"
			WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/man"
			COMMAND ./gen-doc-man.sh qt/bitcoin-qt
		)
		add_custom_target(doc-man-bitcoin-qt
			ALL
			DEPENDS man/bitcoin-qt.1
		)
		install(
			FILES "${CMAKE_CURRENT_BINARY_DIR}/man/bitcoin-qt.1"
			DESTINATION "${MAN_DESTINATION}"
			COMPONENT bitcoin-qt
		)
	endif()
	if(BUILD_BITCOIN_CLI)
		add_custom_command(
			OUTPUT man/bitcoin-cli.1
			DEPENDS bitcoin-cli
			DEPENDS man/footer.h2m
			DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/man/gen-doc-man.sh"
			WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/man"
			COMMAND ./gen-doc-man.sh bitcoin-cli
		)
		add_custom_target(doc-man-bitcoin-cli
			ALL
			DEPENDS man/bitcoin-cli.1
		)
		install(
			FILES "${CMAKE_CURRENT_BINARY_DIR}/man/bitcoin-cli.1"
			DESTINATION "${MAN_DESTINATION}"
			COMPONENT bitcoind
		)
	endif()
	if(BUILD_BITCOIN_TX)
		add_custom_command(
			OUTPUT man/bitcoin-tx.1
			DEPENDS bitcoin-tx
			DEPENDS man/footer.h2m
			DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/man/gen-doc-man.sh"
			WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/man"
			COMMAND ./gen-doc-man.sh bitcoin-tx
		)
		add_custom_target(doc-man-bitcoin-tx
			ALL
			DEPENDS man/bitcoin-tx.1
		)
		install(
			FILES "${CMAKE_CURRENT_BINARY_DIR}/man/bitcoin-tx.1"
			DESTINATION "${MAN_DESTINATION}"
			COMPONENT bitcoind
		)
	endif()
	if(BUILD_BITCOIN_SEEDER)
		add_custom_command(
			OUTPUT man/bitcoin-seeder.1
			DEPENDS bitcoin-seeder
			DEPENDS man/footer.h2m
			DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/man/gen-doc-man.sh"
			WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/man"
			COMMAND ./gen-doc-man.sh seeder/bitcoin-seeder
		)
		add_custom_target(doc-man-bitcoin-seeder
			ALL
			DEPENDS man/bitcoin-seeder.1)
		install(
			FILES "${CMAKE_CURRENT_BINARY_DIR}/man/bitcoin-seeder.1"
			DESTINATION "${MAN_DESTINATION}"
			COMPONENT bitcoin-seeder
		)
	endif()
	add_custom_target(doc-man
		DEPENDS doc-man-bitcoind
		DEPENDS doc-man-bitcoin-qt
		DEPENDS doc-man-bitcoin-cli
		DEPENDS doc-man-bitcoin-tx
		DEPENDS doc-man-bitcoin-seeder
	)
endif()

