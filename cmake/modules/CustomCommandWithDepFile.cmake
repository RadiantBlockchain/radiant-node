# Allow to easily create a custom command that uses dep file.
# depfile are a ninja only feature and a hard fail using other generators.

# define how DEPFILEs are treated
if(${CMAKE_VERSION} VERSION_GREATER "3.19")
  cmake_policy(SET CMP0116 OLD)
endif()

function(add_custom_command_with_depfile)
	cmake_parse_arguments("" "" "DEPFILE" "" ${ARGN})

	if(_DEPFILE AND "${CMAKE_GENERATOR}" MATCHES "Ninja")
		set(_dep_file_arg DEPFILE "${_DEPFILE}")
	endif()

	add_custom_command(${_UNPARSED_ARGUMENTS} ${_dep_file_arg})
endfunction()
