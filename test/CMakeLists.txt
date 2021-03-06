#
# This CMake file is concerned with the testing of the iface library.
#

enable_testing()

# Target without output, running it will run all unit tests
add_custom_target(
  dori-tests
  COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
  USES_TERMINAL)

# Use doctest, a single-header testing library
set(DOCTEST_INCLUDE_DIR ${CMAKE_BINARY_DIR}/doctest)
if(NOT EXISTS "${DOCTEST_INCLUDE_DIR}/doctest.h")
  file(
    DOWNLOAD
    "https://raw.githubusercontent.com/onqtam/doctest/master/doctest/doctest.h"
    ${DOCTEST_INCLUDE_DIR}/doctest.h)
endif()

# Iterate over all .cpp files from this dir
file(GLOB_RECURSE UNIT_TESTS "*.cpp")
foreach(ut IN LISTS UNIT_TESTS)

  # Target name will be the extensionless file name
  get_filename_component(target ${ut} NAME_WLE)

  # Target is a test and dependency on tests
  add_executable(${target} EXCLUDE_FROM_ALL "${ut}")
  add_test(${target} ${target})
  target_link_libraries(${target} dori)
  target_compile_options(${target} PRIVATE /WX /W4)
  target_include_directories(${target} PUBLIC ${DOCTEST_INCLUDE_DIR})
  add_dependencies(dori-tests ${target})

endforeach()
