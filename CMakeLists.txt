cmake_minimum_required(VERSION 3.5)
project(Motivo)

# Set a default build type if none was specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'RelWithDebInfo' as none was specified.")
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Choose the type of build." FORCE)

    # Set the possible values of build type for cmake-gui
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

option(ENABLE_ASSERTS "Enable asserts (slower)" OFF)
option(OPTIMIZE_MORE "Optimize for current hardware" OFF)
option(MOTIVO_OVERFLOW_SAFE "Check for overflows during computation" ON)
option(MOTIVO_MULTITHREAD "Enable multithread support" ON)

if(MOTIVO_MULTITHREAD)
    message(STATUS "Multithreading support enabled.")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -Wconversion -Wall -Wextra -Wpedantic -pedantic-errors -Wdisabled-optimization -Wfloat-equal -Wold-style-cast -Wsign-conversion -Winline -Wzero-as-null-pointer-constant -Wuseless-cast -Wno-zero-as-null-pointer-constant -Wno-unknown-warning-option")

if(ENABLE_ASSERTS)
    message(STATUS "Assertions enabled. Code might be slower.")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -UNDEBUG ")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -UNDEBUG ")
endif()

if(OPTIMIZE_MORE)
    message(STATUS "Setting additional optimization flags. Binaries might not work on other machines")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -funroll-loops -march=native -flto")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -funroll-loops -march=native -flto")
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

check_type_size("uint64_t" MOTIVO_HAS_UINT64_T)
check_type_size("uint128_t" MOTIVO_HAS_UINT128_T)
check_type_size("__uint128_t" MOTIVO_HAS___UINT128_T)

if(NOT MOTIVO_HAS_UINT64_T)
    message(FATAL_ERROR "Compiler does not support required type uint64_t")
endif()

if(MOTIVO_HAS_UINT128_T)
    message(STATUS "Compiler supports type uint128_t")
else()
    message(STATUS "Compiler does not support type uint128_t")

    if(MOTIVO_HAS___UINT128_T)
        message(STATUS "Compiler supports type __uint128_t")
    else()
        message(FATAl_ERROR "Compiler does not support any 128-bit unsigned integer.")
    endif()
endif()

if(MOTIVO_OVERFLOW_SAFE)
    message(STATUS "Building with overflow checks")
endif()

if(MOTIVO_OVERFLOW_SAFE AND NOT MOTIVO_HAS_BUILTIN_ADD_OVERFLOW)
    message(WARNING "No builtin function for checking addition overflow. Will use slow implementation.")
endif()

if(MOTIVO_OVERFLOW_SAFE AND NOT MOTIVO_HAS_BUILTIN_MUL_OVERFLOW)
    message(WARNING "No builtin function for checking multiplication overflow. Will use slow implementation.")
endif()

configure_file(src/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)
include_directories(${CMAKE_BINARY_DIR})

set(COMMON_SOURCE_FILES src/common/UndirectedGraph.cpp src/common/UndirectedGraph.h src/common/Treelet.h src/common/GraphColoring.cpp src/common/GraphColoring.h src/common/Random.h src/common/Treelet.cpp src/common/TreeletTableCollection.cpp src/common/TreeletTableCollection.h src/common/TreeletTable.cpp src/common/TreeletTable.h src/platform/platform.h src/common/AliasMethodSampler.h src/common/OptionsParser.cpp src/common/OptionsParser.h src/common/Random.cpp)
set(MERGER_SOURCE_FILES ${COMMON_SOURCE_FILES} src/merger/merger.cpp)
set(BUILDER_SOURCE_FILES ${COMMON_SOURCE_FILES} src/builder/TreeletTableBuilder.cpp src/builder/TreeletTableBuilder.h src/builder/builder.cpp)
set(SAMPLER_SOURCE_FILES ${COMMON_SOURCE_FILES} src/sampler/TreeletSampler.cpp src/sampler/TreeletSampler.h src/sampler/sampler.cpp src/sampler/include_nauty.h src/sampler/Occurrence.cpp src/sampler/Occurrence.h)

set(TEST_SOURCE_FILES ${COMMON_SOURCE_FILES} tests/tests.cpp tests/Treelet.cpp tests/Treelet.cpp tests/Occurrence.cpp src/sampler/Occurrence.cpp src/sampler/Occurrence.h tests/leftmost_bit_tie.cpp)

string(REPLACE ":" ";" LIBRARY_PATH_STR "$ENV{LIBRARY_PATH}")
set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} ${LIBRARY_PATH_STR})

#https://gcc.gnu.org/onlinedocs/cpp/Environment-Variables.html
string(REPLACE ":" ";" INCLUDE_PATH_STR "$ENV{CPLUS_INCLUDE_PATH}")
set(CMAKE_INCLUDE_PATH ${CMAKE_INCLUDE_PATH} ${INCLUDE_PATH_STR})

string(REPLACE ":" ";" INCLUDE_PATH_STR "$ENV{CPATH}")
set(CMAKE_INCLUDE_PATH ${CMAKE_INCLUDE_PATH} ${INCLUDE_PATH_STR})

find_library(OPENBLAS NAMES openblas)
if(OPENBLAS)
    message(STATUS "openblas library found")
    set(ALGEBRA_LIBRARIES ${ALGEBRA_LIBRARIES} openblas)

    set(OLD_CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES})
    set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} openblas)
    check_cxx_source_compiles("#include <lapacke.h>\n int main() { LAPACKE_dpotrf_work('X', 'X', 0, NULL, 0); }" OPENBLAS_HAS_LAPACKE)
    set(CMAKE_REQUIRED_LIBRARIES ${OLD_CMAKE_REQUIRED_LIBRARIES})
else()
    find_library(CBLAS NAMES cblas)
    if(CBLAS)
        message(STATUS "cblas library found")
        set(ALGEBRA_LIBRARIES ${ALGEBRA_LIBRARIES} cblas)
    else()
        message(FATAL_ERROR "Could not find cblas library")
    endif()
endif()

if(OPENBLAS AND OPENBLAS_HAS_LAPACKE)
    message(STATUS "openblas includes lapacke library")
else()
    find_library(LAPACKE NAMES lapacke)
    if(LAPACKE)
        message(STATUS "lapacke library found")
        set(ALGEBRA_LIBRARIES ${ALGEBRA_LIBRARIES} lapacke)
    else()
        message(FATAL_ERROR "Could not find lapacke library")
    endif()
endif()

find_library(NAUTY NAMES nauty)
if(NOT NAUTY)
    message(FATAL_ERROR "Could not find nauty library")
endif()


find_path(NAUTY_INCLUDE_DIR nauty/nauty.h)
if(NAUTY_INCLUDE_DIR)
   include_directories("${NAUTY_INCLUDE_DIR}")
else()
    message(FATAL_ERROR "Could not find nauty header files")
endif()

find_path(SPARSEHASH_INCLUDE_DIR sparsehash/sparse_hash_map)
if(SPARSEHASH_INCLUDE_DIR)
    include_directories("${SPARSEHASH_INCLUDE_DIR}")
else()
    message(FATAL_ERROR "Could not find sparsehash header files")
endif()

set(COMMON_LIBRARIES m)
set(SAMPLER_LIBRARIES ${COMMON_LIBRARIES} ${ALGEBRA_LIBRARIES} ${NAUTY})
set(BUILDER_LIBRARIES ${COMMON_LIBRARIES})
set(MERGER_LIBRARIES ${COMMON_LIBRARIES})
set(TEST_LIBRARIES ${COMMON_LIBRARIES} ${ALGEBRA_LIBRARIES} ${NAUTY})

add_executable(motivo-build ${BUILDER_SOURCE_FILES})
add_executable(motivo-merge ${MERGER_SOURCE_FILES})
add_executable(motivo-sample ${SAMPLER_SOURCE_FILES})
add_executable(motivo-tests ${TEST_SOURCE_FILES})

target_link_libraries(motivo-build ${BUILDER_LIBRARIES})
target_link_libraries(motivo-merge ${MERGER_LIBRARIES})
target_link_libraries(motivo-sample ${SAMPLER_LIBRARIES})
target_link_libraries(motivo-tests ${TEST_LIBRARIES})

add_executable(motivo-graph src/common/UndirectedGraph.h src/common/UndirectedGraph.cpp src/common/OptionsParser.h src/common/OptionsParser.cpp src/tools/graph.cpp)

file(COPY scripts/build-many.sh DESTINATION .)
file(COPY scripts/loe_to_graph.py DESTINATION .)

INSTALL(TARGETS motivo-build DESTINATION bin)
INSTALL(TARGETS motivo-merge DESTINATION bin)
INSTALL(TARGETS motivo-sample DESTINATION bin)
INSTALL(TARGETS motivo-graph DESTINATION bin)
INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/build-many.sh DESTINATION share/motivo)
INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/loe_to_graph.py DESTINATION share/motivo)

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
