#!/bin/sh
# Arch, Darwin and others just default to "C"
export LC_ALL=C

# Test if system glibc knows about the C.UTF-8 locale
if [ -x `which locale` ] && (locale -a | grep -q "C.UTF-8"); then
	# Debian, Fedora, etc all have this locale
	export LC_ALL=C.UTF-8
fi

# USAGE test_wrapper.sh log executable [args]
# Run the <executable> with supplied <args> arguments.
# The stdout and stderr outputs are redirected to the <log> file, which is only
# printed on error.

LOG="$1"
shift 1

"$@" > "${LOG}" 2>&1 || (cat "${LOG}" && exit 1)
