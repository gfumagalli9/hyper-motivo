list(TRANSFORM COMMON_SOURCE_FILES PREPEND ../common/)

set(MERGER_SOURCE_FILES ${COMMON_SOURCE_FILES} merger.cpp)

add_executable(motivo-merge ${MERGER_SOURCE_FILES})
target_link_libraries(motivo-merge ${COMMON_LIBRARIES})
add_dependencies(motivo-merge git_id)

set_target_properties(motivo-merge PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
