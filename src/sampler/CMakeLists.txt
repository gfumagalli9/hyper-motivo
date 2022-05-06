list(TRANSFORM COMMON_SOURCE_FILES PREPEND ../common/)

set(SAMPLER_SOURCE_FILES
        ${COMMON_SOURCE_FILES}
        TreeletSampler.cpp
        TreeletSampler.h
        sampler_main.cpp
        sampler_opts.h
        sampler_opts.cpp
        OccurrenceSampler.cpp
        OccurrenceSampler.h
        OccurrenceStarSampler.h
        OccurrenceStarSampler.cpp
        SampleTable.h
        SampleTable.cpp
        AdaptiveSampler.h
        AdaptiveSampler.cpp
        ColorCodingSpanningTreeCounter.h
        ColorCodingSpanningTreeCounter.cpp
        Occurrence.cpp
        Occurrence.h
        include_nauty.h
        ValueSortedMap.h
        DynamicSequencer.h
        SpanningTreeCounter.cpp
        SpanningTreeCounter.h
        TimeoutThreadSync.h
        )

add_executable(motivo-sample ${SAMPLER_SOURCE_FILES})
target_link_libraries(motivo-sample ${COMMON_LIBRARIES} ${ALGEBRA_LIBRARIES} ${NAUTY})
add_dependencies(motivo-sample git_id)

set_target_properties(motivo-sample PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
