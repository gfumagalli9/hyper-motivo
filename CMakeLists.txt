cmake_minimum_required(VERSION 3.6)
project(Motivo)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -Wall -Wextra -pedantic")

set(COMMON_SOURCE_FILES src/Graph.cpp src/Graph.h src/treelet.h src/TreeletTable.cpp src/TreeletTable.h src/GraphColoring.cpp src/GraphColoring.h src/Random.h src/treelet.cpp src/TreeletTableCollection.cpp src/TreeletTableCollection.h)
set(TEST_SOURCE_FILES ${COMMON_SOURCE_FILES} tests/tests.cpp tests/treelet.cpp tests/treelet.cpp)
set(MOTIVO_SOURCE_FILES ${COMMON_SOURCE_FILES} src/main.cpp)

set(LIBRARIES gsl gslcblas m)

add_executable(Motivo ${MOTIVO_SOURCE_FILES})
add_executable(Motivo-tests ${TEST_SOURCE_FILES})

target_link_libraries(Motivo ${LIBRARIES})
target_link_libraries(Motivo-tests ${LIBRARIES})