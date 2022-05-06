cmake_minimum_required(VERSION 3.12)
project(Motivo)

set(MOTIVO_VERSION_MAJOR 0)
set(MOTIVO_VERSION_MINOR 1)
set(MOTIVO_VERSION_PATCH 1)

# Set a default build type if none was specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'RelWithDebInfo' as none was specified.")
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Choose the type of build." FORCE)

    # Set the possible values of build type for cmake-gui
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()
set(MOTIVO_BUILD_TYPE CMAKE_BUILD_TYPE)
message(STATUS "CMAKE_BUILD_TYPE = " ${CMAKE_BUILD_TYPE})


option(ENABLE_ASSERTS "Enable asserts (slower)" OFF)
option(OPTIMIZE_MORE "Optimize for current hardware" OFF)
option(MOTIVO_OVERFLOW_SAFE "Check for overflows during computation" ON)
option(MOTIVO_MAY_ALIAS "Turn off if compiler does not support the may_alias attribute" ON)
option(USE_TCMALLOC "Use tcmalloc allocator library" OFF)
option(MOTIVO_DENSE_HASHMAP "Use dense hashmaps" OFF)

set(MOTIVO_FLAGS "")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wconversion -Wall -Wextra -Wpedantic -pedantic-errors -Wdisabled-optimization -Wfloat-equal -Wold-style-cast -Wsign-conversion -Winline -Wzero-as-null-pointer-constant -Wuseless-cast -Wno-zero-as-null-pointer-constant -Wno-unknown-warning-option")

if(ENABLE_ASSERTS)
    message(STATUS "Assertions enabled. Code might be slower.")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 -UNDEBUG ")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -O2 -UNDEBUG ")
    set(MOTIVO_FLAGS "${MOTIVO_FLAGS} EA")
else()
    set(MOTIVO_FLAGS "${MOTIVO_FLAGS} ea")
endif()

if(OPTIMIZE_MORE)
    message(STATUS "Setting additional optimization flags. Binaries might not work on other machines")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -funroll-loops -march=native -flto")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -funroll-loops -march=native -flto")
    set(MOTIVO_FLAGS "${MOTIVO_FLAGS} OM")
else()
    set(MOTIVO_FLAGS "${MOTIVO_FLAGS} om")
endif()

if(NOT MOTIVO_MAY_ALIAS)
    message(STATUS "Assuming compiler does not support the may_alias attribute")
    set(MOTIVO_FLAGS "${MOTIVO_FLAGS} ma")
else()
    set(MOTIVO_FLAGS "${MOTIVO_FLAGS} MA")
endif()

if(MOTIVO_DENSE_HASHMAP)
    message(STATUS "Using dense hashmaps")
    set(MOTIVO_FLAGS "${MOTIVO_FLAGS} DH")
else()
    message(STATUS "Using sparse hashmaps")
    set(MOTIVO_FLAGS "${MOTIVO_FLAGS} dh")
endif()

if(MOTIVO_STAR_SAMPLER_FLOATS)
    message(STATUS "Using floating point distribution for star sampler")
    set(MOTIVO_FLAGS "${MOTIVO_FLAGS} SF")
else()
    set(MOTIVO_FLAGS "${MOTIVO_FLAGS} sf")
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
    set(MOTIVO_FLAGS "${MOTIVO_FLAGS} OS")
else()
    message(STATUS "Building without overflow checks")
    set(MOTIVO_FLAGS "${MOTIVO_FLAGS} os")
endif()

if(MOTIVO_OVERFLOW_SAFE AND NOT MOTIVO_HAS_BUILTIN_ADD_OVERFLOW)
    message(WARNING "No builtin function for checking addition overflow. Will use slow implementation.")
endif()

if(MOTIVO_OVERFLOW_SAFE AND NOT MOTIVO_HAS_BUILTIN_MUL_OVERFLOW)
    message(WARNING "No builtin function for checking multiplication overflow. Will use slow implementation.")
endif()

string(REPLACE ":" ";" LIBRARY_PATH_STR "$ENV{LIBRARY_PATH}")
set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} ${LIBRARY_PATH_STR})

#https://gcc.gnu.org/onlinedocs/cpp/Environment-Variables.html
string(REPLACE ":" ";" INCLUDE_PATH_STR "$ENV{CPLUS_INCLUDE_PATH}")
set(CMAKE_INCLUDE_PATH ${CMAKE_INCLUDE_PATH} ${INCLUDE_PATH_STR})

string(REPLACE ":" ";" INCLUDE_PATH_STR "$ENV{CPATH}")
set(CMAKE_INCLUDE_PATH ${CMAKE_INCLUDE_PATH} ${INCLUDE_PATH_STR})


set(COMMON_LIBRARIES "m")

find_package(Threads REQUIRED)
set(COMMON_LIBRARIES ${COMMON_LIBRARIES} Threads::Threads)

find_library(LZ4 NAMES lz4)
if(NOT LZ4)
    message(FATAL_ERROR "Could not find lz4 library")
endif()
set(COMMON_LIBRARIES ${COMMON_LIBRARIES} ${LZ4})


if(USE_TCMALLOC)
    message(STATUS "Building with tcmalloc allocator")
    find_library(TCMALLOC NAMES tcmalloc)
    if(NOT TCMALLOC)
        message(FATAL_ERROR "Could not find tcmalloc library")
    endif()

    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free")
    set(COMMON_LIBRARIES ${COMMON_LIBRARIES} ${TCMALLOC})
    set(MOTIVO_FLAGS "${MOTIVO_FLAGS} AL")
else()
    set(MOTIVO_FLAGS "${MOTIVO_FLAGS} al")
endif()

find_library(NAUTY NAMES nauty)
if(NOT NAUTY)
    message(FATAL_ERROR "Could not find nauty library")
endif()

find_path(NAUTY_INCLUDE_DIR nauty.h PATH_SUFFIXES nauty)
if(NAUTY_INCLUDE_DIR)
   include_directories("${NAUTY_INCLUDE_DIR}")
else()
    message(FATAL_ERROR "Could not find nauty header files")
endif()

find_path(SPARSEHASH_INCLUDE_DIR sparse_hash_map PATH_SUFFIXES sparsehash google)
if(SPARSEHASH_INCLUDE_DIR)
    include_directories("${SPARSEHASH_INCLUDE_DIR}")
else()
    message(FATAL_ERROR "Could not find sparsehash header files")
endif()




add_custom_target(git_id COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/git_id.cmake)

string(STRIP "${MOTIVO_FLAGS}" MOTIVO_FLAGS)
configure_file(src/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR})


add_subdirectory(src/common/)
add_subdirectory(src/builder/)
add_subdirectory(src/merger/)
add_subdirectory(src/sampler/)
add_subdirectory(src/tools/)
add_subdirectory(src/tests/)

include(testing.cmake)


INSTALL(TARGETS motivo-build DESTINATION bin)
INSTALL(TARGETS motivo-merge DESTINATION bin)
INSTALL(TARGETS motivo-sample DESTINATION bin)
INSTALL(TARGETS motivo-graph DESTINATION bin)
INSTALL(TARGETS motivo-decompose DESTINATION bin)
INSTALL(TARGETS motivo-homomorphism DESTINATION bin)


SET(CPACK_GENERATOR "DEB")
SET(CPACK_PACKAGE_CONTACT "Stefano Leucci")
SET(CPACK_PACKAGE_VERSION_MAJOR ${MOTIVO_VERSION_MAJOR})
SET(CPACK_PACKAGE_VERSION_MINOR ${MOTIVO_VERSION_MINOR})
SET(CPACK_PACKAGE_VERSION_PATCH ${MOTIVO_VERSION_PATCH})
SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Tools for counting and sampling motifs in large graphs")
SET(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://bitbucket.org/steven_/motivo")
SET(CPACK_DEBIAN_PACKAGE_SECTION "science")
SET(CPACK_DEBIAN_PACKAGE_SHLIBDEPS YES)
INCLUDE(CPack)
