
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/src)

IF (WIN32)
    ADD_DEFINITIONS(/DCMAKE_BUILD /D_CRT_SECURE_NO_DEPRECATE)
ENDIF ()

ADD_EXECUTABLE(MQTTClient_pub MQTTClient_pub.c RunningPerformance.c)
ADD_EXECUTABLE(MQTTClient_sub MQTTClient_sub.c)
ADD_EXECUTABLE(MQTTAsync_pub MQTTAsync_pub.c)
ADD_EXECUTABLE(MQTTAsync_sub MQTTAsync_sub.c)
ADD_EXECUTABLE(MQTTAsync_pub_time MQTTAsync_pub_time.c)

TARGET_LINK_LIBRARIES(MQTTClient_pub paho-mqtt3c)
TARGET_LINK_LIBRARIES(MQTTClient_sub paho-mqtt3c)
TARGET_LINK_LIBRARIES(MQTTAsync_pub paho-mqtt3a)
TARGET_LINK_LIBRARIES(MQTTAsync_sub paho-mqtt3a)
TARGET_LINK_LIBRARIES(MQTTAsync_pub_time paho-mqtt3a)

