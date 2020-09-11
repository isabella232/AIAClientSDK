include(../../../cmake/AiaInstall.cmake)

add_library( aiaiotport
             aia_iot_config.c)

target_include_directories( aiaiotport PUBLIC
                            "${PROJECT_SOURCE_DIR}/ports/IoT/include" )

list(APPEND AiaIoT_LIBRARIES
           iotbase
           iotmqtt
           iotserializer
           awsiotcommon)

target_link_libraries( aiaiotport PUBLIC ${AiaIoT_LIBRARIES} )

AiaInstall(aiaiotport)
