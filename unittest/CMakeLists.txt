
    include_directories(${CMAKE_SOURCE_DIR}/dev)

    add_executable(test1 test1.c)
    add_executable(test2 test2.c)


    target_link_libraries(test1 mqtt_client)
    target_link_libraries(test2 mqtt_client)





