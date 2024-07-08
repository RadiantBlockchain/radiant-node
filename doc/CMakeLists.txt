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
	DEPENDS radiantd
	DEPENDS radiant-qt
	DEPENDS radiant-cli
	DEPENDS radiant-tx
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

if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	option(ENABLE_MAN "Enable manpages" ON)
endif()
if(ENABLE_MAN AND BUILD_RADIANT_DAEMON AND NOT WIN32)
	configure_file(
		../cmake/utils/gen-doc-man-footer.sh.in
		man/gen-doc-man-footer.sh
		@ONLY
	)
	add_custom_command(
		OUTPUT man/footer.h2m
		DEPENDS radiantd
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
		OUTPUT man/radiantd.1
		DEPENDS radiantd
		DEPENDS man/footer.h2m
		DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/man/gen-doc-man.sh"
		WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/man"
		COMMAND ./gen-doc-man.sh radiantd
	)
	add_custom_target(doc-man-radiantd
		ALL
		DEPENDS man/radiantd.1
	)
	install(
		FILES "${CMAKE_CURRENT_BINARY_DIR}/man/radiantd.1"
		DESTINATION "${MAN_DESTINATION}"
		COMPONENT radiantd
	)
	if(BUILD_RADIANT_QT)
		add_custom_command(
			OUTPUT man/radiant-qt.1
			DEPENDS radiant-qt
			DEPENDS man/footer.h2m
			DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/man/gen-doc-man.sh"
			WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/man"
			COMMAND ./gen-doc-man.sh qt/radiant-qt
		)
		add_custom_target(doc-man-radiant-qt
			ALL
			DEPENDS man/radiant-qt.1
		)
		install(
			FILES "${CMAKE_CURRENT_BINARY_DIR}/man/radiant-qt.1"
			DESTINATION "${MAN_DESTINATION}"
			COMPONENT radiant-qt
		)
	endif()
	if(BUILD_RADIANT_CLI)
		add_custom_command(
			OUTPUT man/radiant-cli.1
			DEPENDS radiant-cli
			DEPENDS man/footer.h2m
			DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/man/gen-doc-man.sh"
			WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/man"
			COMMAND ./gen-doc-man.sh radiant-cli
		)
		add_custom_target(doc-man-radiant-cli
			ALL
			DEPENDS man/radiant-cli.1
		)
		install(
			FILES "${CMAKE_CURRENT_BINARY_DIR}/man/radiant-cli.1"
			DESTINATION "${MAN_DESTINATION}"
			COMPONENT radiantd
		)
	endif()
	if(BUILD_RADIANT_TX)
		add_custom_command(
			OUTPUT man/radiant-tx.1
			DEPENDS radiant-tx
			DEPENDS man/footer.h2m
			DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/man/gen-doc-man.sh"
			WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/man"
			COMMAND ./gen-doc-man.sh radiant-tx
		)
		add_custom_target(doc-man-radiant-tx
			ALL
			DEPENDS man/radiant-tx.1
		)
		install(
			FILES "${CMAKE_CURRENT_BINARY_DIR}/man/radiant-tx.1"
			DESTINATION "${MAN_DESTINATION}"
			COMPONENT radiantd
		)
	endif()
	if(BUILD_RADIANT_SEEDER)
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
		DEPENDS doc-man-radiantd
		DEPENDS doc-man-radiant-qt
		DEPENDS doc-man-radiant-cli
		DEPENDS doc-man-radiant-tx
		DEPENDS doc-man-bitcoin-seeder
	)
endif()

