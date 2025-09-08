# hypergraph_testing.cmake
include(CTest)
enable_testing()

# 1) costruzione ipergrafo da testo
add_test(
  NAME build-hgraph
  COMMAND motivo-hypergraph --input ${CMAKE_SOURCE_DIR}/graphs/test-hypergraph.txt --output test-hg
  WORKING_DIRECTORY tests
)

add_test(
  NAME dedup-hgraph
  COMMAND motivo-hgdedup test-hg test-hypergraph
  WORKING_DIRECTORY tests
)

# 2) proiezione Gaifman
add_test(
  NAME build-gaifman
  COMMAND motivo-gaifman --input test-hypergraph --output test-gaif
  WORKING_DIRECTORY tests
)

# 3) size=1, grafi ipergrafo single-threaded
add_test(
  NAME motivo-hypergraph-build-1
  COMMAND motivo-build --hyper -g test-hypergraph -s 1 -c 5 -o test --seed 42
  WORKING_DIRECTORY tests
)
add_test(
  NAME motivo-hypergraph-merge-1
  COMMAND motivo-merge -o test.1 test.1.cnt
  WORKING_DIRECTORY tests
)
add_test(
  NAME motivo-hypergraph-nws-1
  COMMAND motivo-nws -g test-hypergraph -s 1 -i test -o test
  WORKING_DIRECTORY tests
)
add_test(
  NAME motivo-hypergraph-merge-ie-1
  COMMAND motivo-merge -e -o test.1.ie test.1.ie.cnt
  WORKING_DIRECTORY tests
)

# 4) size = 2..5, grafi ipergrafo single-threaded
foreach(size RANGE 2 5)
    math(EXPR prev "${size}-1")

    add_test(
      NAME motivo-hypergraph-build-${size}
      COMMAND motivo-build --hyper -g test-hypergraph -i test -s ${size} -c 5 -o test --seed 42
      WORKING_DIRECTORY tests
    )
    add_test(
      NAME motivo-hypergraph-merge-${size}
      COMMAND motivo-merge -o test.${size} test.${size}.cnt
      WORKING_DIRECTORY tests
    )
    add_test(
      NAME motivo-hypergraph-nws-${size}
      COMMAND motivo-nws -g test-hypergraph -s ${size} -i test -o test
      WORKING_DIRECTORY tests
    )
    add_test(
      NAME motivo-hypergraph-merge-ie-${size}
      COMMAND motivo-merge -e --output test.${size}.ie test.${size}.ie.cnt
      WORKING_DIRECTORY tests
    )
endforeach()

# 5) size=1, multithread build on Gaifman-projected graph
add_test(
  NAME motivo-hypergraph-build-gaif-1
  COMMAND motivo-build -g test-gaif -s 1 -c 5 -o test-gaif --seed 42
  WORKING_DIRECTORY tests
)
add_test(
  NAME motivo-hypergraph-merge-gaif-1
  COMMAND motivo-merge -o test-gaif.1 test-gaif.1.cnt
  WORKING_DIRECTORY tests
)

foreach(size RANGE 2 5)
    math(EXPR prev "${size}-1")

    add_test(
      NAME motivo-hypergraph-build-gaif-${size}
      COMMAND motivo-build -g test-gaif -s ${size} -i test-gaif -o test-gaif --threads 1
      WORKING_DIRECTORY tests
    )
    add_test(
      NAME motivo-hypergraph-merge-gaif-${size}
      COMMAND motivo-merge -o test-gaif.${size} test-gaif.${size}.cnt
      WORKING_DIRECTORY tests
    )

    add_test(
      NAME motivo-hypergraph-dtz-gaif-matches-hg-${size}
      COMMAND ${CMAKE_COMMAND} -E compare_files test-gaif.${size}.dtz test.${size}.dtz
      WORKING_DIRECTORY tests
    )
    add_test(
      NAME motivo-hypergraph-rts-gaif-matches-hg-${size}
      COMMAND ${CMAKE_COMMAND} -E compare_files test-gaif.${size}.rts test.${size}.rts
      WORKING_DIRECTORY tests
    )
endforeach()