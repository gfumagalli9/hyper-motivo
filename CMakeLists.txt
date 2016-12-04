cmake_minimum_required(VERSION 3.6)
project(Motivo)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse4 -std=c++14 -Wconversion -Wall -Wextra -Wpedantic -pedantic-errors")

SET(CMAKE_CXX_FLAGS_RELEASE "-DNDEBUG -O4 -march=native")

set(COMMON_SOURCE_FILES src/UndirectedGraph.cpp src/UndirectedGraph.h src/Treelet.h src/GraphColoring.cpp src/GraphColoring.h src/Random.h src/Treelet.cpp src/TreeletTableCollection.cpp src/TreeletTableCollection.h src/TreeletTable.cpp src/TreeletTable.h src/bit_cast.h src/platform.h)
set(BUILDER_SOURCE_FILES ${COMMON_SOURCE_FILES} src/builder/TreeletTableBuilder.cpp src/builder/TreeletTableBuilder.h src/builder/builder.cpp)
set(SAMPLER_SOURCE_FILES ${COMMON_SOURCE_FILES} src/sampler/TreeletSampler.cpp src/sampler/TreeletSampler.h src/ReservoirSampler.h src/sampler/KirchhoffSpanningTreeCounter.cpp src/sampler/KirchhoffSpanningTreeCounter.h src/sampler/sampler.cpp src/sampler/include_nauty.h src/sampler/GraphFootprint.cpp src/sampler/GraphFootprint.h)

set(TEST_SOURCE_FILES ${COMMON_SOURCE_FILES} tests/tests.cpp tests/Treelet.cpp tests/Treelet.cpp tests/KirchhoffSpanningTreeCounter.cpp src/sampler/KirchhoffSpanningTreeCounter.cpp src/sampler/KirchhoffSpanningTreeCounter.h tests/leftmost_bit_tie.cpp)


set(COMMON_LIBRARIES cmph m)
set(SAMPLER_LIBRARIES ${COMMON_LIBRARIES} openblas lapacke nauty)
set(BUILDER_LIBRARIES ${COMMON_LIBRARIES})
set(TEST_LIBRARIES ${COMMON_LIBRARIES} openblas lapacke)

add_executable(motivo-builder ${BUILDER_SOURCE_FILES})
add_executable(motivo-sampler ${SAMPLER_SOURCE_FILES})
add_executable(motivo-tests ${TEST_SOURCE_FILES})

target_link_libraries(motivo-builder ${BUILDER_LIBRARIES})
target_link_libraries(motivo-sampler ${SAMPLER_LIBRARIES})
target_link_libraries(motivo-tests ${TEST_LIBRARIES})

add_executable(motivo-graph2bin src/UndirectedGraph.cpp src/tools/graph2bin.cpp)