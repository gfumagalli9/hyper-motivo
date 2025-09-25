# CMakeLists.txt per motivo-hyper-build

# Prepend della radice 'common' (stessa convenzione del tuo builder classico)
list(TRANSFORM COMMON_SOURCE_FILES PREPEND ../common/)

# Sorgenti specifici dell'hyper builder
set(HYPER_BUILDER_SOURCE_FILES
        ${COMMON_SOURCE_FILES}
        main.cpp          # nuovo main separato
        HyperSequentialBuilder.cpp
        HyperSequentialBuilder.h
        HyperMultithreadedBuilder.h
        HyperMultithreadedBuilder.cpp
        InclusionExclusionBuilder.h          # contiene IEBuilder (header-only)
)

add_executable(motivo-hyper-build ${HYPER_BUILDER_SOURCE_FILES})
target_link_libraries(motivo-hyper-build ${COMMON_LIBRARIES})
add_dependencies(motivo-hyper-build git_id)

set_target_properties(motivo-hyper-build PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)