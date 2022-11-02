
include_directories(${CMAKE_CURRENT_LIST_DIR}/utils)

aux_source_directory(${CMAKE_CURRENT_LIST_DIR}/utils UTILS_SRC_LIST)
aux_source_directory(${CMAKE_CURRENT_LIST_DIR} SRC_LIST)

add_executable(mqtt_client_pub ${SRC_LIST} ${UTILS_SRC_LIST} mqttclient.c mqttclient.h mqttpacket.c mqttpacket.h clients.c clients.h mqtttime.c mqtttime.h)

target_link_libraries(mqtt_client_pub pthread)