list(TRANSFORM COMMON_SOURCE_FILES PREPEND ../common/)

set(GRAPH_SOURCE_FILES ${COMMON_SOURCE_FILES} graph.cpp)
set(DECOMPOSE_SOURCE_FILES ${COMMON_SOURCE_FILES} decompose.cpp)
set(HOMOMORPHISM_SOURCE_FILES ${COMMON_SOURCE_FILES} homomorphism.cpp)
set(TREELET_COUNTS_SOURCE_FILES ${COMMON_SOURCE_FILES} treelet-counts.cpp)

add_executable(motivo-graph ${GRAPH_SOURCE_FILES})
add_executable(motivo-decompose ${DECOMPOSE_SOURCE_FILES})
add_executable(motivo-homomorphism ${HOMOMORPHISM_SOURCE_FILES})
add_executable(motivo-treelet-counts ${TREELET_COUNTS_SOURCE_FILES})

target_link_libraries(motivo-graph ${COMMON_LIBRARIES})
target_link_libraries(motivo-decompose ${COMMON_LIBRARIES})
target_link_libraries(motivo-homomorphism ${COMMON_LIBRARIES})
target_link_libraries(motivo-treelet-counts ${COMMON_LIBRARIES})

add_dependencies(motivo-graph git_id)
add_dependencies(motivo-decompose git_id)
add_dependencies(motivo-homomorphism git_id)
add_dependencies(motivo-treelet-counts git_id)

set_target_properties(motivo-graph PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set_target_properties(motivo-decompose PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set_target_properties(motivo-homomorphism PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set_target_properties(motivo-treelet-counts PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
