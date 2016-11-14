cmake_minimum_required(VERSION 3.6)
project(Motivo)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Wextra -pedantic")

set(SOURCE_FILES main.cpp Graph.cpp Graph.h treelet.h TreeletTable.cpp TreeletTable.h GraphColoring.cpp GraphColoring.h Random.h treelet.cpp)
add_executable(Motivo ${SOURCE_FILES})

target_link_libraries(Motivo gsl gslcblas m)