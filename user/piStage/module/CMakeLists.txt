include_directories(include)
aux_source_directory(src MODULE_SRC)

if(NOT EUDAQ_TTREE_LIBRARY)
  list(REMOVE_ITEM MODULE_SRC src/Ex0RawEvent2TTreeEventConverter.cc)
endif()

add_library(${EUDAQ_MODULE} SHARED ${MODULE_SRC})
target_link_libraries(${EUDAQ_MODULE}
                      ${EUDAQ_CORE_LIBRARY}
                      ${USER_HARDWARE_LIBRARY})

install(TARGETS
  ${EUDAQ_MODULE}
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib)
