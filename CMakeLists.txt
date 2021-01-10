#
# This is the top-level CMake file. It specifies global configuration and
# references subdirectories containing individual targets of the project.
#

cmake_minimum_required(VERSION 3.14)

project(
  dori
  VERSION 0.0.1
  LANGUAGES CXX)

get_directory_property(parent PARENT_DIRECTORY)
if(NOT parent)

  # Use vcpkg as a submodule
  set(CMAKE_TOOLCHAIN_FILE
      ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake
      CACHE STRING "Vcpkg toolchain file")
  include(${CMAKE_TOOLCHAIN_FILE})

  # Use C++20
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)

elseif(CMAKE_CXX_STANDARD LESS 20 OR NOT CMAKE_CXX_STANDARD_REQUIRED)
  message("dori uses C++20 features, reconsider the C++ standard settings")
endif()

add_subdirectory("dori")

# Optionally add tests
option(DORI_TESTS
       "generates a custom target called dori-tests that runs all tests")
if(DORI_TESTS)
  add_subdirectory("test")
endif()
