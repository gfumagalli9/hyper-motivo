list(TRANSFORM COMMON_SOURCE_FILES PREPEND ../common/)

set(GRAPH_SOURCE_FILES ${COMMON_SOURCE_FILES} graph.cpp)
set(DECOMPOSE_SOURCE_FILES ${COMMON_SOURCE_FILES} decompose.cpp)
set(HOMOMORPHISM_SOURCE_FILES ${COMMON_SOURCE_FILES} homomorphism.cpp)
set(TREELET_COUNTS_SOURCE_FILES ${COMMON_SOURCE_FILES} treelet-counts.cpp)
set(GAIFAMN_SOURCE_FILES ${COMMON_SOURCE_FILES} gaifman.cpp)
set(HYPERGRAPH_SOURCE_FILES ${COMMON_SOURCE_FILES} hypergraph.cpp)
set(HGSPLIT_SOURCE_FILES ${COMMON_SOURCE_FILES} hgsplit.cpp)
set(HGDEDUP_SOURCE_FILES ${COMMON_SOURCE_FILES} hgdedup.cpp)
set(temp ${COMMON_SOURCE_FILES} ${BUILDER_SOURCE_FILES} temp.cpp)


add_executable(motivo-graph ${GRAPH_SOURCE_FILES})
add_executable(motivo-decompose ${DECOMPOSE_SOURCE_FILES})
add_executable(motivo-homomorphism ${HOMOMORPHISM_SOURCE_FILES})
add_executable(motivo-treelet-counts ${TREELET_COUNTS_SOURCE_FILES})
add_executable(motivo-gaifman ${GAIFAMN_SOURCE_FILES})
add_executable(motivo-hypergraph ${HYPERGRAPH_SOURCE_FILES})
add_executable(motivo-hgsplit ${HGSPLIT_SOURCE_FILES})
add_executable(motivo-hgdedup ${HGDEDUP_SOURCE_FILES})


target_link_libraries(motivo-graph ${COMMON_LIBRARIES})
target_link_libraries(motivo-decompose ${COMMON_LIBRARIES})
target_link_libraries(motivo-homomorphism ${COMMON_LIBRARIES})
target_link_libraries(motivo-treelet-counts ${COMMON_LIBRARIES})
target_link_libraries(motivo-gaifman ${COMMON_LIBRARIES})
target_link_libraries(motivo-hypergraph ${COMMON_LIBRARIES})
target_link_libraries(motivo-hgsplit ${COMMON_LIBRARIES})
target_link_libraries(motivo-hgdedup ${COMMON_LIBRARIES})


add_dependencies(motivo-graph git_id)
add_dependencies(motivo-decompose git_id)
add_dependencies(motivo-homomorphism git_id)
add_dependencies(motivo-treelet-counts git_id)
add_dependencies(motivo-gaifman git_id)
add_dependencies(motivo-hypergraph git_id)
add_dependencies(motivo-hgsplit git_id)
add_dependencies(motivo-hgdedup git_id)


set_target_properties(motivo-graph PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set_target_properties(motivo-decompose PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set_target_properties(motivo-homomorphism PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set_target_properties(motivo-treelet-counts PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set_target_properties(motivo-gaifman PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set_target_properties(motivo-hypergraph PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set_target_properties(motivo-hgsplit PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set_target_properties(motivo-hgdedup PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

