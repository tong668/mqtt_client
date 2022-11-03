cmake_minimum_required(VERSION 3.10)
project(mqtt_client C)

set(CMAKE_C_STANDARD 99)

set(PAHO_USE_SELECT TRUE CACHE BOOL "Revert to select system call instead of poll")
set(PAHO_HIGH_PERFORMANCE TRUE CACHE BOOL "Disable tracing and heap tracking")
set(MQTT_DEV TRUE CACHE BOOL "Disable tracing and heap tracking")


IF (PAHO_USE_SELECT)
    add_definitions(-DUSE_SELECT=1)
ENDIF ()

#取消memory tracking
IF (PAHO_HIGH_PERFORMANCE)
    add_definitions(-DHIGH_PERFORMANCE=1)
ENDIF ()

#取消离线消息持久化
add_definitions(-DNO_PERSISTENCE=1)

add_subdirectory(src)
add_subdirectory(sample)

#正在开发的代码
#add_subdirectory(dev)



