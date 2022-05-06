execute_process(COMMAND hg id -i -b WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} TIMEOUT 5 RESULT_VARIABLE HG_ID_RESULT OUTPUT_VARIABLE HG_ID OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT HG_ID_RESULT EQUAL 0)
    set(HG_ID "unknown")
else()
    execute_process(COMMAND hg log -b . -r . --template "{date(localdate(date,0),'%Y-%m-%dT%H:%MZ')}" WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} TIMEOUT 5 RESULT_VARIABLE HG_DATE_RESULT OUTPUT_VARIABLE HG_DATE OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(HG_DATE_RESULT EQUAL 0)
        set(HG_ID "${HG_ID} ${HG_DATE}")
    endif()
endif()


message(STATUS "Detected hg id: ${HG_ID}")

file(WRITE hg_id.h.tmp "#define MOTIVO_HG_ID \"${HG_ID}\"")
execute_process(COMMAND ${CMAKE_COMMAND}  -E copy_if_different  hg_id.h.tmp hg_id.h)
