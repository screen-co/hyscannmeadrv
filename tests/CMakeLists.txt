
set (TEST_LIBRARIES ${GLIB2_LIBRARIES}
                    ${HYSCAN_LIBRARIES})

add_executable (nmea-uart-test nmea-uart-test.c)
add_executable (nmea-udp-test nmea-udp-test.c)
add_executable (nmea-uart2udp nmea-uart2udp.c)
add_executable (nmea-drv-test nmea-drv-test.c)

target_link_libraries (nmea-uart-test ${TEST_LIBRARIES} ${HYSCAN_NMEA_DRV})
target_link_libraries (nmea-udp-test ${TEST_LIBRARIES} ${HYSCAN_NMEA_DRV})
target_link_libraries (nmea-uart2udp ${TEST_LIBRARIES} ${HYSCAN_NMEA_DRV})
target_link_libraries (nmea-drv-test ${TEST_LIBRARIES})

install (TARGETS nmea-uart-test
                 nmea-udp-test
                 nmea-uart2udp
                 nmea-drv-test
         COMPONENT test
         RUNTIME DESTINATION bin
         LIBRARY DESTINATION lib
         ARCHIVE DESTINATION lib
         PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
