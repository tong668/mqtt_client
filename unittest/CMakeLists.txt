
IF (NOT MQTT_DEV)
    include_directories(${CMAKE_SOURCE_DIR}/src)

    add_executable(test1 test1.c)
    add_executable(test2 test2.c)
#    add_executable(test10 test10.c)
    add_executable(thread_test thread_test.c)
    add_executable(test4 test4.c)

    target_link_libraries(test1 mqtt_client)
    target_link_libraries(test2 mqtt_client)
#    target_link_libraries(test10 mqtt_client)
    target_link_libraries(thread_test mqtt_client)
    target_link_libraries(test4 mqtt_client)
ELSE ()
    include_directories(${CMAKE_SOURCE_DIR}/dev)

    add_executable(test1 test1.c)
    add_executable(test2 test2.c)
#    add_executable(test10 test10.c)
    add_executable(thread_test thread_test.c)
    add_executable(test4 test4.c)

    target_link_libraries(test1 mqtt_client_dev)
    target_link_libraries(test2 mqtt_client_dev)
#    target_link_libraries(test10 mqtt_client_dev)
    target_link_libraries(thread_test mqtt_client_dev)
    target_link_libraries(test4 mqtt_client_dev)
ENDIF ()




