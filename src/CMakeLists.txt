
include_directories(${CMAKE_CURRENT_LIST_DIR}/utils)

aux_source_directory(${CMAKE_CURRENT_LIST_DIR}/utils UTILS_SRC_LIST)
aux_source_directory(${CMAKE_CURRENT_LIST_DIR} SRC_LIST)

add_library(mqtt_client ${SRC_LIST} ${UTILS_SRC_LIST})

target_link_libraries(mqtt_client pthread)