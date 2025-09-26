list(TRANSFORM COMMON_SOURCE_FILES PREPEND ../common/)

set(BUILDER_SOURCE_FILES
        ${COMMON_SOURCE_FILES}
        main.cpp
        MultithreadedBuilder.cpp
        MultithreadedBuilder.h
        SequentialBuilder.h
        Size1Builder.h
        Size1Builder.cpp
        SequentialBuilder.cpp
        ColorCodingBuilder.h
        ColorCodingHashmap.h
        )

add_executable(motivo-build ${BUILDER_SOURCE_FILES})
target_link_libraries(motivo-build ${COMMON_LIBRARIES})
add_dependencies(motivo-build git_id)

set_target_properties(motivo-build PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
