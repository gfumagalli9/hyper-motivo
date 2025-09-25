# CMake for motivo-hyper-sample (non-adaptive hypergraph sampler)

# Se il progetto principale definisce già COMMON_SOURCE_FILES/COMMON_LIBRARIES, le riutilizziamo.
# Altrimenti puoi commentare le 2 righe seguenti.
list(TRANSFORM COMMON_SOURCE_FILES PREPEND ../common/)

set(HYPER_SAMPLER_SOURCE_FILES
    ${COMMON_SOURCE_FILES}

    # ---- hyper_sampler-local ----
    HyperTreeletSampler.cpp
    HyperTreeletSampler.h
    HyperOccurrenceSampler.cpp
    HyperOccurrenceSampler.h
    HyperOccurrence.cpp
    HyperOccurrence.h
    HyperSampleTable.cpp      
    HyperSampleTable.h
    hyper_sampler_main.cpp

    # ---- riuso componenti dal sampler classico ----
    ../sampler/Occurrence.cpp
    ../sampler/Occurrence.h
    ../sampler/SampleTable.cpp
    ../sampler/SampleTable.h
    ../sampler/ColorCodingSpanningTreeCounter.cpp
    ../sampler/ColorCodingSpanningTreeCounter.h
    ../sampler/SpanningTreeCounter.cpp
    ../sampler/SpanningTreeCounter.h
    ../sampler/ValueSortedMap.h
    ../sampler/DynamicSequencer.h
    ../sampler/TimeoutThreadSync.h
    ../sampler/include_nauty.h
)

add_executable(motivo-hyper-sample ${HYPER_SAMPLER_SOURCE_FILES})

# Se servono include aggiuntivi (dipende dall'albero del progetto):
target_include_directories(motivo-hyper-sample PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../sampler
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
)

# Link: eredita dalle var del progetto principale
target_link_libraries(motivo-hyper-sample
    ${COMMON_LIBRARIES}
    ${ALGEBRA_LIBRARIES}
    ${NAUTY}
)

add_dependencies(motivo-hyper-sample git_id)

set_target_properties(motivo-hyper-sample PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)