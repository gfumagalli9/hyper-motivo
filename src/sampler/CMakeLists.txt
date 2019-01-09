set(SAMPLER_SOURCE_FILES
        ${COMMON_SOURCE_FILES}
        src/sampler/TreeletSampler.cpp
        src/sampler/TreeletSampler.h
        src/sampler/sampler_main.cpp
        src/sampler/sampler_opts.h
        src/sampler/sampler_opts.cpp
        src/sampler/OccurrenceSampler.cpp
        src/sampler/OccurrenceSampler.h
        src/sampler/SpanningTreeCounter.cpp
        src/sampler/SpanningTreeCounter.h
        src/sampler/OccurrenceStarSampler.h
        src/sampler/OccurrenceStarSampler.cpp
        src/sampler/SampleTable.h
        src/sampler/SampleTable.cpp
        src/sampler/AdaptiveSampler.h
        src/sampler/AdaptiveSampler.cpp
        src/sampler/ColorCodingSpanningTreeCounter.h
        src/sampler/ColorCodingSpanningTreeCounter.cpp
        src/sampler/Occurrence.cpp
        src/sampler/Occurrence.h
        src/sampler/include_nauty.h
        src/sampler/ValueSortedMap.h
        src/sampler/CachedSTC.h
        src/sampler/CachedSTC.cpp
        )

add_executable(motivo-sample ${SAMPLER_SOURCE_FILES})
target_link_libraries(motivo-sample ${COMMON_LIBRARIES} ${ALGEBRA_LIBRARIES} ${NAUTY})
add_dependencies(motivo-sample hg_id)
