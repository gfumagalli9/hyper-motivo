set(GRAPH_SOURCE_FILES
        ${COMMON_SOURCE_FILES}
        src/tools/graph.cpp
        )

set(DECOMPOSE_SOURCE_FILES
        ${COMMON_SOURCE_FILES}
        src/tools/decompose.cpp
        )

set(HOMOMORPHISM_SOURCE_FILES
        ${COMMON_SOURCE_FILES}
        src/tools/homomorphism.cpp
        )

set(TREELET_COUNTS_SOURCE_FILES
        ${COMMON_SOURCE_FILES}
        src/tools/treelet-counts.cpp
        )

add_executable(motivo-graph ${GRAPH_SOURCE_FILES})
add_executable(motivo-decompose ${DECOMPOSE_SOURCE_FILES})
add_executable(motivo-homomorphism ${HOMOMORPHISM_SOURCE_FILES})
add_executable(motivo-treelet-counts ${TREELET_COUNTS_SOURCE_FILES})

target_link_libraries(motivo-graph ${COMMON_LIBRARIES})
target_link_libraries(motivo-decompose ${COMMON_LIBRARIES})
target_link_libraries(motivo-homomorphism ${COMMON_LIBRARIES})
target_link_libraries(motivo-treelet-counts ${COMMON_LIBRARIES})

add_dependencies(motivo-graph hg_id)
add_dependencies(motivo-decompose hg_id)
add_dependencies(motivo-homomorphism hg_id)
add_dependencies(motivo-treelet-counts hg_id)
