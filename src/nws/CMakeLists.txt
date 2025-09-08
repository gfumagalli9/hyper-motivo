# CMakeLists.txt per motivo-nws

# Prendi tutti i sorgenti comuni da ../common
list(TRANSFORM COMMON_SOURCE_FILES PREPEND ../common/)

# Elenco dei sorgenti per motivo-nws
set(NWS_SOURCE_FILES
    ${COMMON_SOURCE_FILES}
    main.cpp
    SequentialNWSBuilder.cpp
    SequentialNWSBuilder.h
    MultithreadNWSBuilder.cpp
    MultithreadNWSBuilder.h
)

# Crea l'eseguibile
add_executable(motivo-nws ${NWS_SOURCE_FILES})
# Linka la libreria comune (definita altrove)
target_link_libraries(motivo-nws ${COMMON_LIBRARIES})
# Assicurati di rigenerare quando cambia il git id target
add_dependencies(motivo-nws git_id)

# Metti l'eseguibile in bin/
set_target_properties(motivo-nws PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")