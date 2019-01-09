set(MERGER_SOURCE_FILES
        ${COMMON_SOURCE_FILES}
        src/merger/merger.cpp
        )

add_executable(motivo-merge ${MERGER_SOURCE_FILES})
target_link_libraries(motivo-merge ${COMMON_LIBRARIES})
add_dependencies(motivo-merge hg_id)
