include(CTest)
enable_testing()

add_test(NAME build-graph COMMAND motivo-graph --input ${CMAKE_SOURCE_DIR}/graphs/test-graph.txt --output test-graph WORKING_DIRECTORY tests)
add_test(NAME motivo-tests COMMAND motivo-tests "-tce=*slow" WORKING_DIRECTORY tests)
set_tests_properties(motivo-tests PROPERTIES DEPENDS build-graph)


add_test(NAME motivo-build-1 COMMAND motivo-build -g test-graph -s 1 -c 5 -o test --seed 42 WORKING_DIRECTORY tests)
set_tests_properties(motivo-build-1 PROPERTIES DEPENDS build-graph)

add_test(NAME motivo-merge-1 COMMAND motivo-merge -o test.1 test.1.cnt WORKING_DIRECTORY tests)
set_tests_properties(motivo-merge-1 PROPERTIES DEPENDS motivo-build-1)

foreach(size RANGE 2 5)
    math(EXPR prev ${size}-1)

    add_test(NAME motivo-build-${size} COMMAND motivo-build -g test-graph -s ${size} -i test -o test WORKING_DIRECTORY tests)
    set_tests_properties(motivo-build-${size} PROPERTIES DEPENDS motivo-merge-${prev})

    add_test(NAME motivo-merge-${size} COMMAND motivo-merge -o test.${size} test.${size}.cnt WORKING_DIRECTORY tests)
    set_tests_properties(motivo-merge-${size} PROPERTIES DEPENDS motivo-build-${size})
endforeach(size)

add_test(NAME motivo-build-mt-1 COMMAND motivo-build -g test-graph -s 1 -c 5 -o test-mt --seed 42 WORKING_DIRECTORY tests)
set_tests_properties(motivo-build-mt-1 PROPERTIES DEPENDS build-graph)

add_test(NAME motivo-merge-mt-1 COMMAND motivo-merge -o test-mt.1 test.1.cnt WORKING_DIRECTORY tests)
set_tests_properties(motivo-merge-mt-1 PROPERTIES DEPENDS motivo-build-mt-1)

foreach(size RANGE 2 5)
    math(EXPR prev ${size}-1)

    add_test(NAME motivo-build-mt-${size} COMMAND motivo-build -g test-graph -s ${size} -i test-mt -o test-mt --threads 0 WORKING_DIRECTORY tests)
    set_tests_properties(motivo-build-mt-${size} PROPERTIES DEPENDS motivo-merge-mt-${prev})

    add_test(NAME motivo-merge-mt-${size} COMMAND motivo-merge -o test-mt.${size} test-mt.${size}.cnt WORKING_DIRECTORY tests)
    set_tests_properties(motivo-merge-mt-${size} PROPERTIES DEPENDS motivo-build-mt-${size})

    add_test(NAME motivo-dtz-mt-matches-st-${size} COMMAND ${CMAKE_COMMAND} -E compare_files test-mt.${size}.dtz test.${size}.dtz WORKING_DIRECTORY tests)
    set_tests_properties(motivo-dtz-mt-matches-st-${size} PROPERTIES DEPENDS motivo-merge-mt-${size})

    add_test(NAME motivo-rts-mt-matches-st-${size} COMMAND ${CMAKE_COMMAND} -E compare_files test-mt.${size}.rts test.${size}.rts WORKING_DIRECTORY tests)
    set_tests_properties(motivo-rts-mt-matches-st-${size} PROPERTIES DEPENDS motivo-merge-mt-${size})
endforeach(size)

