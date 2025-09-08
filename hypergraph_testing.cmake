# hypergraph_testing.cmake
include(CTest)
enable_testing()

# 1) costruzione ipergrafo da testo
add_test(
  NAME build-hgraph
  COMMAND motivo-hypergraph --input ${CMAKE_SOURCE_DIR}/graphs/test-hypergraph.txt --output test-hypergraph
  WORKING_DIRECTORY tests
)


#add_test(
#  NAME dedup-hgraph
#  COMMAND motivo-hgdedup test-hg test-hypergraph
#  WORKING_DIRECTORY tests
#)

# 2) proiezione Gaifman
add_test(
  NAME build-gaifman
  COMMAND motivo-gaifman --input test-hypergraph --output test-gaif
  WORKING_DIRECTORY tests
)

# Split ipergrafo
add_test(
  NAME split-hgraph
  COMMAND motivo-hgsplit -i test-hypergraph -s test-gaifman-low -l test-hypergraph-high -t 3 
  WORKING_DIRECTORY tests
)

# Gaifman grafo low
add_test(
  NAME build-gaifman-low
  COMMAND motivo-gaifman --input test-gaifman-low --output test-gaifman-low # nome input uguale a output per pairs
  WORKING_DIRECTORY tests
)

# 3) Size 1: coloro ipergrafo high e gaifman low
# High
add_test(
  NAME motivo-hypergraph-high-build-1
  COMMAND motivo-build --hyper -g test-hypergraph-high -s 1 -c 5 -o test-high --seed 42
  WORKING_DIRECTORY tests
)
add_test(
  NAME motivo-hypergraph-high-merge-1
  COMMAND motivo-merge -o test-high.1 test-high.1.cnt
  WORKING_DIRECTORY tests
)
add_test(
  NAME motivo-hypergraph-high-nws-1
  COMMAND motivo-nws -g test-hypergraph-high -s 1 -i test-high -o test-high
  WORKING_DIRECTORY tests
)
add_test(
  NAME motivo-hypergraph-high-merge-ie-1
  COMMAND motivo-merge -e -o test.1.ie test-high.1.ie.cnt
  WORKING_DIRECTORY tests
)

# Low
add_test(
  NAME motivo-hypergraph-low-build-1
  COMMAND motivo-build -g test-gaifman-low -s 1 -c 5 -o test-low --seed 42
  WORKING_DIRECTORY tests
)
add_test(
  NAME motivo-hypergraph-low-merge-1
  COMMAND motivo-merge -o test-low.1 test-low.1.cnt
  WORKING_DIRECTORY tests
)

# "Fondo" le tabelle di dimensione 1 (in realtà qui non posso sovracontare, quindi semplicemente copio una delle tabelle già presenti)
add_test(
  NAME motivo-hypergraph-merge-1
  COMMAND motivo-merge -o test.1 test-high.1.cnt
  WORKING_DIRECTORY tests
)


# 4) size = 2..5, grafi ipergrafo single-threaded
foreach(size RANGE 2 5)
    math(EXPR prev "${size}-1")

    # High
    add_test(
      NAME motivo-hypergraph-build-${size}
      COMMAND motivo-build --hyper -g test-hypergraph-high -i test -s ${size} -o test-high --normalize false
      WORKING_DIRECTORY tests
    )
    add_test(
      NAME motivo-hypergraph-merge-${size}
      COMMAND motivo-merge -o test-high.${size} test-high.${size}.cnt
      WORKING_DIRECTORY tests
    )

    # Low
    add_test(
      NAME motivo-hypergraph-low-build-${size}
      COMMAND motivo-build -g test-gaifman-low -s ${size} -i test -o test-low --normalize false
      WORKING_DIRECTORY tests
    )
    add_test(
      NAME motivo-hypergraph-low-merge-${size}
      COMMAND motivo-merge -o test-low.${size} test-low.${size}.cnt
      WORKING_DIRECTORY tests
    )

    # Fondo
    add_test(
      NAME motivo-hypergraph-low-high-${size}
      COMMAND motivo-low-high-merge --low test-low.${size}.cnt --high test-high.${size}.cnt -o test.${size} -c test-gaifman-low
      WORKING_DIRECTORY tests
    )
    add_test(
      NAME motivo-hypergraph-low-high-merge-${size}
      COMMAND motivo-merge -o test.${size} test.${size}.cnt
      WORKING_DIRECTORY tests
    )

    # NWS
    add_test(
      NAME motivo-hypergraph-nws-${size}
      COMMAND motivo-nws -g test-hypergraph-high -s ${size} -i test -o test-high
      WORKING_DIRECTORY tests
    )
    add_test(
      NAME motivo-hypergraph-merge-ie-${size}
      COMMAND motivo-merge -e --output test.${size}.ie test-high.${size}.ie.cnt
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