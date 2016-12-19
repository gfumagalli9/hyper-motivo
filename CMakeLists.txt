cmake_minimum_required(VERSION 3.6)
project(Motivo)

# Set a default build type if none was specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'Release' as none was specified.")
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)

    # Set the possible values of build type for cmake-gui
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse4 -std=c++14 -Wconversion -Wall -Wextra -Wpedantic -pedantic-errors -Wdisabled-optimization -Wfloat-equal -Wold-style-cast -Wsign-conversion -Winline -Wzero-as-null-pointer-constant -Wuseless-cast -Wno-unknown-warning-option")

if(OPTIMIZE_MORE)
    message(STATUS "Setting additional optimization flags. Binaries might not work on other machines")
    set(CMAKE_CXX_FLAGS_RELEASE "-DNDEBUG -O4 -funroll-loops -march=native -flto")
endif()

INCLUDE(CheckCXXSourceCompiles)
check_cxx_source_compiles("int main() { __builtin_popcount(0xFFFFFFFF); return 0; } " MOTIVO_HAS_BUILTIN_POPCOUNT)
check_cxx_source_compiles("int main() { __builtin_popcountl(0xFFFFFFFF); return 0; } " MOTIVO_HAS_BUILTIN_POPCOUNTL)
check_cxx_source_compiles("int main() { int a; __builtin_add_overflow(0, 0, &a); return 0; } " MOTIVO_HAS_BUILTIN_ADD_OVERFLOW)
check_cxx_source_compiles("int main() { int a; __builtin_mul_overflow(0, 0, &a); return 0; } " MOTIVO_HAS_BUILTIN_MUL_OVERFLOW)

INCLUDE(CheckTypeSize)
check_type_size("short" MOTIVO_SHORT_SIZE LANGUAGE CXX)
check_type_size("int" MOTIVO_INT_SIZE LANGUAGE CXX)
check_type_size("long" MOTIVO_LONG_SIZE LANGUAGE CXX)
check_type_size("long long" MOTIVO_LONG_LONG_SIZE LANGUAGE CXX)

set(MOTIVO_OVERFLOW_SAFE ON)

if(MOTIVO_OVERFLOW_SAFE AND NOT MOTIVO_HAS_BUILTIN_ADD_OVERFLOW)
    message(WARNING "No builtin function for checking addition overflow. Will use slow implementation.")
endif()

if(MOTIVO_OVERFLOW_SAFE AND NOT MOTIVO_HAS_BUILTIN_MUL_OVERFLOW)
    message(WARNING "No builtin function for checking multiplication overflow. Will use slow implementation.")
endif()

configure_file(src/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

include_directories(${CMAKE_BINARY_DIR})

set(COMMON_SOURCE_FILES src/common/UndirectedGraph.cpp src/common/UndirectedGraph.h src/common/Treelet.h src/common/GraphColoring.cpp src/common/GraphColoring.h src/common/Random.h src/common/Treelet.cpp src/common/TreeletTableCollection.cpp src/common/TreeletTableCollection.h src/common/TreeletTable.cpp src/common/TreeletTable.h src/platform.h src/common/AliasMethodSampler.cpp src/common/AliasMethodSampler.h)
set(MERGER_SOURCE_FILES ${COMMON_SOURCE_FILES} src/merger/merger.cpp)
set(BUILDER_SOURCE_FILES ${COMMON_SOURCE_FILES} src/builder/TreeletTableBuilder.cpp src/builder/TreeletTableBuilder.h src/builder/builder.cpp)
set(SAMPLER_SOURCE_FILES ${COMMON_SOURCE_FILES} src/sampler/TreeletSampler.cpp src/sampler/TreeletSampler.h src/sampler/ReservoirSampler.h src/sampler/sampler.cpp src/sampler/include_nauty.h src/sampler/Occurrence.cpp src/sampler/Occurrence.h)

set(TEST_SOURCE_FILES ${COMMON_SOURCE_FILES} tests/tests.cpp tests/Treelet.cpp tests/Treelet.cpp tests/Occurrence.cpp src/sampler/Occurrence.cpp src/sampler/Occurrence.h tests/leftmost_bit_tie.cpp)


set(COMMON_LIBRARIES m)
set(SAMPLER_LIBRARIES ${COMMON_LIBRARIES} boost_program_options openblas lapacke nauty)
set(BUILDER_LIBRARIES ${COMMON_LIBRARIES} boost_program_options)
set(MERGER_LIBRARIES ${COMMON_LIBRARIES} boost_program_options)
set(TEST_LIBRARIES ${COMMON_LIBRARIES} openblas lapacke lapacke nauty)

add_executable(motivo-build ${BUILDER_SOURCE_FILES})
add_executable(motivo-merge ${MERGER_SOURCE_FILES})
add_executable(motivo-sample ${SAMPLER_SOURCE_FILES})
add_executable(motivo-tests ${TEST_SOURCE_FILES})

target_link_libraries(motivo-build ${BUILDER_LIBRARIES})
target_link_libraries(motivo-merge ${MERGER_LIBRARIES})
target_link_libraries(motivo-sample ${SAMPLER_LIBRARIES})
target_link_libraries(motivo-tests ${TEST_LIBRARIES})

add_executable(motivo-graph src/common/UndirectedGraph.cpp src/tools/graph.cpp)
target_link_libraries(motivo-graph boost_program_options)

file(COPY scripts/build-many.sh DESTINATION .)

INSTALL(TARGETS motivo-build DESTINATION bin)
INSTALL(TARGETS motivo-merge DESTINATION bin)
INSTALL(TARGETS motivo-sample DESTINATION bin)
INSTALL(TARGETS motivo-graph DESTINATION bin)
INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/build-many.sh DESTINATION share/motivo)

enable_testing()
add_test(NAME build-graph COMMAND motivo-graph --input ../graphs/test-graph.txt --output test-graph)
add_test(NAME motivo-tests COMMAND motivo-tests -tce=leftmost_bit_tie)
set_tests_properties(motivo-tests PROPERTIES DEPENDS build-graph)

SET(CPACK_GENERATOR "DEB")
SET(CPACK_PACKAGE_CONTACT "Stefano Leucci")
SET(CPACK_PACKAGE_VERSION_MAJOR "0")
SET(CPACK_PACKAGE_VERSION_MINOR "1")
SET(CPACK_PACKAGE_VERSION_PATCH "1")
SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Tools for counting and sampling motifs in large graphs")
SET(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://bitbucket.org/steven_/motivo")
SET(CPACK_DEBIAN_PACKAGE_SECTION "science")
SET(CPACK_DEBIAN_PACKAGE_SHLIBDEPS YES)
INCLUDE(CPack)
