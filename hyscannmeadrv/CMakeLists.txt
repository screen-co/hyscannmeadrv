
find_program (MARSHAL_COMPILER glib-genmarshal)
if (${MARSHAL_COMPILER} STREQUAL MARSHAL_COMPILER-NOTFOUND)
  message (FATAL_ERROR "GLib marshallers compiler not found.")
endif ()

file (MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/marshallers)
include_directories (${CMAKE_BINARY_DIR}/marshallers)

add_custom_command (OUTPUT "${CMAKE_BINARY_DIR}/marshallers/hyscan-nmea-marshallers.c"
                    COMMAND ${MARSHAL_COMPILER} --header
                            --prefix hyscan_nmea_marshal
                            hyscan-nmea-marshallers.list >
                            "${CMAKE_BINARY_DIR}/marshallers/hyscan-nmea-marshallers.h"
                    COMMAND ${MARSHAL_COMPILER} --body
                            --prefix hyscan_nmea_marshal
                            hyscan-nmea-marshallers.list >
                            "${CMAKE_BINARY_DIR}/marshallers/hyscan-nmea-marshallers.c"
                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                    DEPENDS hyscan-nmea-marshallers.list
                    VERBATIM)

configure_file ("hyscan-nmea-drv.h.in" "${CMAKE_BINARY_DIR}/configured/hyscan-nmea-drv.h")
include_directories ("${CMAKE_BINARY_DIR}/configured/")

add_library (${HYSCAN_NMEA_DRV} SHARED
             hyscan-nmea-receiver.c
             hyscan-nmea-uart.c
             hyscan-nmea-udp.c
             hyscan-nmea-driver.c
             hyscan-nmea-discover.c
             hyscan-nmea-drv.c
             "${CMAKE_BINARY_DIR}/marshallers/hyscan-nmea-marshallers.c")

target_link_libraries (${HYSCAN_NMEA_DRV}
                       ${WIN32_LIBRARIES}
                       ${GLIB2_LIBRARIES}
                       ${MATH_LIBRARIES}
                       ${HYSCAN_LIBRARIES})

set_target_properties (${HYSCAN_NMEA_DRV} PROPERTIES DEFINE_SYMBOL "HYSCAN_API_EXPORTS")
set_target_properties (${HYSCAN_NMEA_DRV} PROPERTIES PREFIX "")
set_target_properties (${HYSCAN_NMEA_DRV} PROPERTIES SUFFIX ".drv")

install (TARGETS ${HYSCAN_NMEA_DRV}
         COMPONENT runtime
         RUNTIME DESTINATION bin
         LIBRARY DESTINATION lib
         ARCHIVE DESTINATION lib
         PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
