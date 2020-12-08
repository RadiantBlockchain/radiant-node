# Copyright (c) 2020 The Bitcoin developers

project(bitcoin-lint)

include(TestSuite)
create_test_suite(lint)
add_dependencies(check check-lint)

add_lint_script_tests_to_suite(lint
	TESTS
	lint-circular-dependencies.sh
	lint-boost-dependencies.sh
	check-doc.py
)
