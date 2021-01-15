option(BUILD_SOURCE_CONTROL_TOOLS "Build the source control tools" OFF)
if(BUILD_SOURCE_CONTROL_TOOLS)
	add_subdirectory(source-control-tools)
endif()

option(BUILD_ASERT_TEST_VECTORS "Create ASERT test vector targets" OFF)
if (BUILD_ASERT_TEST_VECTORS)
	add_subdirectory(testgen)
endif()

add_subdirectory(devtools)

include(PackageHelper)
exclude_from_source_package(
	# Subdirectories
	"debian/"
	"gitian/"
	"gitian-descriptors/"
	"qos/"
	"seeds/"
	"testgen/"
	"zmq/"

	# Files
	"bitcoin-qt.pro"
	"gitian-build.py"
	"README.md"
	"valgrind.supp"
)
