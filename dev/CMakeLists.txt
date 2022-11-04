
#include_directories(${CMAKE_CURRENT_LIST_DIR}/utils)
#
#aux_source_directory(${CMAKE_CURRENT_LIST_DIR}/utils UTILS_SRC_LIST)
#aux_source_directory(${CMAKE_CURRENT_LIST_DIR} SRC_LIST)
#
#add_executable(mqtt_client_dev ${SRC_LIST} ${UTILS_SRC_LIST})
#
#target_link_libraries(mqtt_client_dev pthread)