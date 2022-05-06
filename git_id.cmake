execute_process(COMMAND git describe --always --dirty WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} TIMEOUT 5 RESULT_VARIABLE GIT_ID_RESULT OUTPUT_VARIABLE GIT_ID OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT GIT_ID_RESULT EQUAL 0)
    set(GIT_ID "unknown")
endif()


message(STATUS "Detected git id: ${GIT_ID}")

file(WRITE git_id.h.tmp "#define MOTIVO_GIT_ID \"${GIT_ID}\"")
execute_process(COMMAND ${CMAKE_COMMAND}  -E copy_if_different  git_id.h.tmp git_id.h)
