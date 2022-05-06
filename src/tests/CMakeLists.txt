list(TRANSFORM COMMON_SOURCE_FILES PREPEND ../common/)

set(TEST_SOURCE_FILES
        ${COMMON_SOURCE_FILES}
        tests.cpp
        Treelet.cpp
        Treelet.cpp
        leftmost_bit_tie.cpp
        CompressedRecordFile.cpp
        SpanningTreeCounter.cpp
        ../sampler/Occurrence.h
        ../sampler/Occurrence.cpp
        ../sampler/include_nauty.h
        ../sampler/ColorCodingSpanningTreeCounter.h
        ../sampler/ColorCodingSpanningTreeCounter.cpp
        ../sampler/SpanningTreeCounter.h
        ../sampler/SpanningTreeCounter.cpp
        )

add_executable(motivo-tests ${TEST_SOURCE_FILES})
target_link_libraries(motivo-tests ${COMMON_LIBRARIES} ${ALGEBRA_LIBRARIES} ${NAUTY})
add_dependencies(motivo-tests git_id)

set_target_properties(motivo-tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
