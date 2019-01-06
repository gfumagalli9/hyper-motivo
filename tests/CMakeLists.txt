set(TEST_SOURCE_FILES
${COMMON_SOURCE_FILES}
tests/tests.cpp
        tests/Treelet.cpp
        tests/Treelet.cpp
        tests/Occurrence.cpp
        tests/ColorCodingSpanningTreeCounter.cpp
        tests/leftmost_bit_tie.cpp
        tests/CompressedRecordFile.cpp
        tests/SpanningTreeCounter.cpp
        src/sampler/Occurrence.h
        src/sampler/Occurrence.cpp
        src/sampler/include_nauty.h
        src/sampler/SpanningTreeCounter.h
        src/sampler/SpanningTreeCounter.cpp
        src/sampler/ColorCodingSpanningTreeCounter.h
        src/sampler/ColorCodingSpanningTreeCounter.cpp
        )

add_executable(motivo-tests ${TEST_SOURCE_FILES})
target_link_libraries(motivo-tests ${COMMON_LIBRARIES} ${ALGEBRA_LIBRARIES} ${NAUTY})
add_dependencies(motivo-tests hg_id)