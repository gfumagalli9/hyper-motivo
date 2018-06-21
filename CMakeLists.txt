cmake_minimum_required(VERSION 3.5)
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

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wconversion -Wall -Wextra -Wpedantic -pedantic-errors -Wdisabled-optimization -Wfloat-equal -Wold-style-cast -Wsign-conversion -Winline -Wzero-as-null-pointer-constant -Wuseless-cast -Wno-zero-as-null-pointer-constant -Wno-unknown-warning-option")

if(ENABLE_ASSERTS)
    message(STATUS "Assertions enabled. Code might be slower.")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -UNDEBUG ")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -UNDEBUG ")
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
    set(MOTIVO_FLAGS "${MOTIVO_FLAGS} os")
endif()

if(MOTIVO_OVERFLOW_SAFE AND NOT MOTIVO_HAS_BUILTIN_ADD_OVERFLOW)
    message(WARNING "No builtin function for checking addition overflow. Will use slow implementation.")
endif()

if(MOTIVO_OVERFLOW_SAFE AND NOT MOTIVO_HAS_BUILTIN_MUL_OVERFLOW)
    message(WARNING "No builtin function for checking multiplication overflow. Will use slow implementation.")
endif()

set(COMMON_SOURCE_FILES src/common/graph/UndirectedGraph.cpp src/common/graph/UndirectedGraph.h src/common/treelets/Treelet.h src/common/graph/GraphColoring.cpp src/common/graph/GraphColoring.h src/common/graph/FullGraphColoring.h src/common/graph/FullGraphColoring.cpp src/common/Random.h src/common/treelets/Treelet.cpp src/common/treelets/TreeletTableCollection.cpp src/common/treelets/TreeletTableCollection.h src/common/treelets/TreeletTable.cpp src/common/treelets/TreeletTable.h src/platform/platform.h src/platform/platform.cpp src/common/AliasMethodSampler.h src/common/OptionsParser.cpp src/common/OptionsParser.h src/common/io/CompressedRecordFile.cpp src/common/io/CompressedRecordFile.h src/common/io/RecordCompressor.cpp src/common/io/RecordCompressor.h src/common/io/BaseRecordSource.h src/common/sequencer/BaseSequencer.h src/common/sequencer/StaticSequencer.h src/common/sequencer/DynamicSequencer.h src/common/io/ConcurrentWriter.cpp src/common/io/ConcurrentWriter.h src/common/treelets/TreeletSelector.h src/common/RangeSampler.h)
set(MERGER_SOURCE_FILES ${COMMON_SOURCE_FILES} src/merger/merger.cpp)
set(BUILDER_SOURCE_FILES ${COMMON_SOURCE_FILES} src/builder/TreeletTableBuilder.cpp src/builder/TreeletTableBuilder.h src/common/io/ConcurrentFIFO.h src/builder/builder_main.cpp src/builder/builder.cpp src/builder/builder.h)
set(SAMPLER_SOURCE_FILES ${COMMON_SOURCE_FILES} src/sampler/TreeletSampler.cpp src/sampler/TreeletSampler.h src/sampler/sampler_main.cpp src/sampler/include_nauty.h src/sampler/Occurrence.cpp src/sampler/Occurrence.h src/sampler/sampler_opts.h src/sampler/sampler_opts.cpp src/sampler/OccurrenceSampler.cpp src/sampler/OccurrenceSampler.h src/common/SpanningTreeCounter.cpp src/common/SpanningTreeCounter.h src/builder/SimpleTreeletTableBuilder.cpp src/builder/SimpleTreeletTableBuilder.h src/sampler/OccurrenceStarSampler.h src/sampler/OccurrenceStarSampler.cpp src/common/DiscreteDistribution.h)
set(GRAPH_SOURCE_FILES ${COMMON_SOURCE_FILES} src/tools/graph.cpp)
set(DECOMPOSE_SOURCE_FILES ${COMMON_SOURCE_FILES} src/tools/decompose.cpp)
set(HOMOMORPHISM_SOURCE_FILES ${COMMON_SOURCE_FILES} src/tools/homomorphism.cpp)
set(TREELET_COUNTS_SOURCE_FILES  ${COMMON_SOURCE_FILES} src/tools/treelet-counts.cpp)

set(TEST_SOURCE_FILES ${COMMON_SOURCE_FILES} tests/tests.cpp tests/Treelet.cpp tests/Treelet.cpp tests/Occurrence.cpp src/sampler/Occurrence.cpp src/sampler/Occurrence.h tests/leftmost_bit_tie.cpp tests/CompressedRecordFile.cpp tests/UndirectedGraph.cpp tests/SpanningTreeCounter.cpp src/common/SpanningTreeCounter.cpp src/common/SpanningTreeCounter.h src/builder/SimpleTreeletTableBuilder.cpp src/builder/SimpleTreeletTableBuilder.h)

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


set(SAMPLER_LIBRARIES ${COMMON_LIBRARIES} ${ALGEBRA_LIBRARIES} ${NAUTY})
set(BUILDER_LIBRARIES ${COMMON_LIBRARIES})
set(MERGER_LIBRARIES ${COMMON_LIBRARIES})
set(GRAPH_LIBRARIES ${COMMON_LIBRARIES})
set(DECOMPOSE_LIBRARIES ${COMMON_LIBRARIES})
set(HOMOMORPHISM_LIBRARIES ${COMMON_LIBRARIES})
set(TREELET_COUNTS_LIBRARIES ${COMMON_LIBRARIES})

set(TEST_LIBRARIES ${COMMON_LIBRARIES} ${ALGEBRA_LIBRARIES} ${NAUTY})


add_executable(motivo-build ${BUILDER_SOURCE_FILES})
add_executable(motivo-merge ${MERGER_SOURCE_FILES})
add_executable(motivo-sample ${SAMPLER_SOURCE_FILES})
add_executable(motivo-graph ${GRAPH_SOURCE_FILES})
add_executable(motivo-decompose ${DECOMPOSE_SOURCE_FILES})
add_executable(motivo-homomorphism ${HOMOMORPHISM_SOURCE_FILES})
add_executable(motivo-treelet-counts ${TREELET_COUNTS_SOURCE_FILES})

add_executable(motivo-tests ${TEST_SOURCE_FILES})

target_link_libraries(motivo-build ${BUILDER_LIBRARIES})
target_link_libraries(motivo-merge ${MERGER_LIBRARIES})
target_link_libraries(motivo-sample ${SAMPLER_LIBRARIES})
target_link_libraries(motivo-graph ${GRAPH_LIBRARIES})
target_link_libraries(motivo-decompose ${DECOMPOSE_LIBRARIES})
target_link_libraries(motivo-homomorphism ${HOMOMORPHISM_LIBRARIES})
target_link_libraries(motivo-treelet-counts ${TREELET_COUNTS_LIBRARIES})

target_link_libraries(motivo-tests ${TEST_LIBRARIES})

add_custom_target(hg_id COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/hg_id.cmake)

add_dependencies(motivo-build hg_id)
add_dependencies(motivo-merge hg_id)
add_dependencies(motivo-sample hg_id)
add_dependencies(motivo-decompose hg_id)
add_dependencies(motivo-homomorphism hg_id)
add_dependencies(motivo-treelet-counts hg_id)
add_dependencies(motivo-tests hg_id)



find_package(MPI)

if(MPI_FOUND)
    set(COMMON_MPI_SOURCE_FILES src/mpi/protocol.h src/mpi/MotivoMPIContext.h src/mpi/MotivoMPIContext.cpp src/mpi/MPISequencer.h src/mpi/MPIRemoteCompressedRecordFile.h)

    set(TABLESERVER_SOURCE_FILES ${COMMON_SOURCE_FILES} ${COMMON_MPI_SOURCE_FILES} src/mpi/tableserver.cpp )
    set(MASTERBUILDER_SOURCE_FILES ${COMMON_SOURCE_FILES} ${COMMON_MPI_SOURCE_FILES} src/builder/mpi/master_builder.cpp src/builder/builder.h src/builder/builder.cpp)
    set(SLAVEBUILDER_SOURCE_FILES ${COMMON_SOURCE_FILES} ${COMMON_MPI_SOURCE_FILES} src/builder/mpi/slave_builder.cpp src/builder/builder.h src/builder/builder.cpp src/builder/TreeletTableBuilder.h src/builder/TreeletTableBuilder.cpp)
    set(MASTERSAMPLER_SOURCE_FILES ${COMMON_SOURCE_FILES} ${COMMON_MPI_SOURCE_FILES} src/sampler/mpi/master_sampler.cpp src/sampler/sampler_opts.h src/sampler/sampler_opts.cpp)
    set(SLAVESAMPLER_SOURCE_FILES ${COMMON_SOURCE_FILES} ${COMMON_MPI_SOURCE_FILES} src/sampler/mpi/slave_sampler.cpp src/sampler/sampler_opts.h src/sampler/sampler_opts.cpp src/sampler/Occurrence.h src/sampler/Occurrence.cpp src/sampler/TreeletSampler.h src/sampler/TreeletSampler.cpp)

    set(TABLESERVER_LIBRARIES ${COMMON_LIBRARIES} ${MPI_LIBRARIES})
    set(MASTERBUILDER_LIBRARIES  ${COMMON_LIBRARIES} ${MPI_LIBRARIES})
    set(SLAVEBUILDER_LIBRARIES  ${BUILDER_LIBRARIES} ${MPI_LIBRARIES})
    set(MASTERSAMPLER_LIBRARIES  ${COMMON_LIBRARIES} ${MPI_LIBRARIES})
    set(SLAVESAMPLER_LIBRARIES  ${SAMPLER_LIBRARIES} ${MPI_LIBRARIES})

    add_executable(motivo-tableserver ${TABLESERVER_SOURCE_FILES})
    add_executable(motivo-build-master ${MASTERBUILDER_SOURCE_FILES})
    add_executable(motivo-build-slave ${SLAVEBUILDER_SOURCE_FILES})
    add_executable(motivo-sample-master ${MASTERSAMPLER_SOURCE_FILES})
    add_executable(motivo-sample-slave ${SLAVESAMPLER_SOURCE_FILES})

    target_link_libraries(motivo-tableserver ${TABLESERVER_LIBRARIES})
    target_link_libraries(motivo-build-master ${MASTERBUILDER_LIBRARIES})
    target_link_libraries(motivo-build-slave ${SLAVEBUILDER_LIBRARIES})
    target_link_libraries(motivo-sample-master ${MASTERSAMPLER_LIBRARIES})
    target_link_libraries(motivo-sample-slave ${SLAVESAMPLER_LIBRARIES})

    include_directories(SYSTEM ${MPI_INCLUDE_PATH})
    set_target_properties(motivo-tableserver PROPERTIES COMPILE_FLAGS "${MPI_COMPILE_FLAGS}")
    set_target_properties(motivo-build-master  PROPERTIES COMPILE_FLAGS "${MPI_COMPILE_FLAGS}")
    set_target_properties(motivo-build-slave PROPERTIES LINK_FLAGS "${MPI_LINK_FLAGS}")
    set_target_properties(motivo-sample-master PROPERTIES LINK_FLAGS "${MPI_LINK_FLAGS}")
    set_target_properties(motivo-sample-slave PROPERTIES LINK_FLAGS "${MPI_LINK_FLAGS}")

    add_dependencies(motivo-tableserver hg_id)
    add_dependencies(motivo-build-master hg_id)
    add_dependencies(motivo-build-slave hg_id)
    add_dependencies(motivo-sample-master hg_id)
    add_dependencies(motivo-sample-slave hg_id)
endif()


file(COPY scripts/build-and-sample.sh DESTINATION .)
file(COPY scripts/build-many.sh DESTINATION .)
file(COPY scripts/loe_to_graph.py DESTINATION .)

INSTALL(TARGETS motivo-build DESTINATION bin)
INSTALL(TARGETS motivo-merge DESTINATION bin)
INSTALL(TARGETS motivo-sample DESTINATION bin)
INSTALL(TARGETS motivo-graph DESTINATION bin)
INSTALL(TARGETS motivo-decompose DESTINATION bin)
INSTALL(TARGETS motivo-homomorphism DESTINATION bin)
INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/build-many.sh DESTINATION share/motivo)
INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/build-and-sample.sh DESTINATION share/motivo)
INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/loe_to_graph.py DESTINATION share/motivo)

if(MPI_FOUND)
    INSTALL(TARGETS motivo-build-master DESTINATION bin)
    INSTALL(TARGETS motivo-build-slave DESTINATION bin)
    INSTALL(TARGETS motivo-sample-master DESTINATION bin)
    INSTALL(TARGETS motivo-sample-slave DESTINATION bin)
    INSTALL(TARGETS motivo-tableserver DESTINATION bin)
endif()

string(STRIP "${MOTIVO_FLAGS}" MOTIVO_FLAGS)
configure_file(src/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

enable_testing()
add_test(NAME build-graph COMMAND motivo-graph --input ../graphs/test-graph.txt --output test-graph)
add_test(NAME motivo-tests COMMAND motivo-tests "-tce=leftmost_bit_tie,CompressedRecordFile Multiblock")
set_tests_properties(motivo-tests PROPERTIES DEPENDS build-graph)

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
