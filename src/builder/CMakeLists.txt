set(BUILDER_SOURCE_FILES
        ${COMMON_SOURCE_FILES}
        src/builder/main.cpp
        src/builder/MultithreadedBuilder.cpp
        src/builder/MultithreadedBuilder.h
        src/builder/SequentialBuilder.h
        src/builder/Size1Builder.h
        src/builder/Size1Builder.cpp
        src/builder/SequentialBuilder.cpp
        src/builder/ColorCodingHashmap.h
        src/builder/ColorCodingBuilder.h
        src/builder/SimpleMultithreadedBuilder.cpp
        src/builder/SimpleMultithreadedBuilder.h
        )

add_executable(motivo-build ${BUILDER_SOURCE_FILES})
target_link_libraries(motivo-build ${COMMON_LIBRARIES})
add_dependencies(motivo-build hg_id)
