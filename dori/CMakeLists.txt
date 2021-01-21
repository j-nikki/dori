#
# This is the CMake file for the target 'dori', a header-only library.
#

add_library(dori INTERFACE)

find_path(BOOST_ALIGN_INCLUDE_DIRS "boost/align.hpp")
target_include_directories(dori INTERFACE ${BOOST_ALIGN_INCLUDE_DIRS})
find_path(BOOST_MP11_INCLUDE_DIRS "boost/mp11.hpp")
target_include_directories(dori INTERFACE ${BOOST_MP11_INCLUDE_DIRS})
find_path(BOOST_PREPROCESSOR_INCLUDE_DIRS "boost/preprocessor.hpp")
target_include_directories(dori INTERFACE ${BOOST_PREPROCESSOR_INCLUDE_DIRS})
target_include_directories(dori INTERFACE "include")

if(DORI_ADD_NATVIS)
  target_sources(dori INTERFACE dori.natvis)
endif()
