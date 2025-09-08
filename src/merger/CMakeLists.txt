list(TRANSFORM COMMON_SOURCE_FILES PREPEND ../common/)

set(MERGE_SOURCE_FILES ${COMMON_SOURCE_FILES} merger.cpp)
set(LOW_HIGH_MERGE_SOURCE_FILES ${COMMON_SOURCE_FILES} low_high_merger.cpp)


add_executable(motivo-merge ${MERGE_SOURCE_FILES})
target_link_libraries(motivo-merge ${COMMON_LIBRARIES})
add_dependencies(motivo-merge git_id)

set_target_properties(motivo-merge PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_executable(motivo-low-high-merge ${LOW_HIGH_MERGE_SOURCE_FILES})
target_link_libraries(motivo-low-high-merge  ${COMMON_LIBRARIES})
add_dependencies(motivo-low-high-merge  git_id)

set_target_properties(motivo-low-high-merge  PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
