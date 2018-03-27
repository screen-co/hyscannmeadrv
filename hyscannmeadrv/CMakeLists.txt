
add_library (${HYSCAN_NMEA_DRV} SHARED
             hyscan-nmea.c)

target_link_libraries (${HYSCAN_NMEA_DRV}
                       ${GLIB2_LIBRARIES}
                       ${ZLIB_LIBRARIES}
                       ${MATH_LIBRARIES}
                       ${HYSCAN_LIBRARIES})

set_target_properties (${HYSCAN_NMEA_DRV} PROPERTIES PREFIX "")
set_target_properties (${HYSCAN_NMEA_DRV} PROPERTIES SUFFIX ".drv")

install (TARGETS ${HYSCAN_NMEA_DRV}
         COMPONENT runtime
         RUNTIME DESTINATION bin
         LIBRARY DESTINATION lib
         ARCHIVE DESTINATION lib
         PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
