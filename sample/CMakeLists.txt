
    include_directories(${CMAKE_SOURCE_DIR}/src)
    add_executable(mqtt_pub mqtt_pub.c)
    add_executable(mqtt_sub mqtt_sub.c)


    target_link_libraries(mqtt_pub mqtt_client)
    target_link_libraries(mqtt_sub mqtt_client)


