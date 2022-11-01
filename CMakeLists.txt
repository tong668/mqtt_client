cmake_minimum_required(VERSION 3.10)
project(mqtt_lite C)

set(CMAKE_C_STANDARD 99)

SET(PAHO_USE_SELECT TRUE CACHE BOOL "Revert to select system call instead of poll")
SET(PAHO_HIGH_PERFORMANCE TRUE CACHE BOOL "Disable tracing and heap tracking")

SET(MQTT_DEV TRUE CACHE BOOL "Switch dev or paho src code")

IF (PAHO_USE_SELECT)
    ADD_DEFINITIONS(-DUSE_SELECT=1)
ENDIF ()

IF (PAHO_HIGH_PERFORMANCE)
    ADD_DEFINITIONS(-DHIGH_PERFORMANCE=1)
ENDIF ()

IF (NOT MQTT_DEV)
    #paho源码，不包括openssl和MQTTAsync相关功能
    add_subdirectory(src)
ELSE ()
    #正在开发的代码
    add_subdirectory(dev)
ENDIF ()

#sample和单元测试
add_subdirectory(sample)
add_subdirectory(unittest)


