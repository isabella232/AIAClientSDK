include(../../cmake/AiaInstall.cmake)

# MQTT demo source files.
add_executable( aia_demo
                main.c
                ${PROJECT_SOURCE_DIR}/demos/source/aia_sample_app.c )

# Select the demo function for the MQTT demo.
target_compile_definitions( aia_demo
                            PRIVATE RunDemo=RunAiaDemo )

# Aia demo library dependencies.
target_link_libraries( aia_demo PRIVATE aiacore aiaport aiaregulator aiasequencer aiaspeakermanager aiamicrophonemanager aiaregistrationmanager aiaclient )

target_include_directories( aia_demo PUBLIC ${PROJECT_SOURCE_DIR}/AiaCore/include ${PROJECT_SOURCE_DIR}/demos/source ${PROJECT_SOURCE_DIR}/ApplicationUtilities/aiaclient/include )

target_include_directories(aia_demo PUBLIC ${PROJECT_SOURCE_DIR}/ApplicationUtilities/aiaalerttonesynthesizer/include)

if(AIA_PORTAUDIO_MICROPHONE)
add_definitions("-DAIA_PORTAUDIO_MICROPHONE")
target_link_libraries( aia_demo PRIVATE aiaportaudiomicrophone )
target_include_directories(aia_demo PUBLIC ${PROJECT_SOURCE_DIR}/ApplicationUtilities/aiaportaudiomicrophone/include)
endif()

if(AIA_PORTAUDIO_SPEAKER)
add_definitions("-DAIA_PORTAUDIO_SPEAKER")
target_link_libraries( aia_demo PRIVATE aiaportaudiospeaker )
target_include_directories(aia_demo PUBLIC ${PROJECT_SOURCE_DIR}/ApplicationUtilities/aiaportaudiospeaker/include)
endif()

if(AIA_OPUS_DECODER)
add_definitions("-DAIA_OPUS_DECODER")
target_link_libraries( aia_demo PRIVATE aiaopusdecoder )
target_include_directories(aia_demo PUBLIC ${PROJECT_SOURCE_DIR}/ApplicationUtilities/aiaopusdecoder/include)
endif()

if(AIA_LIBCURL_HTTP_CLIENT)
add_definitions("-DAIA_LIBCURL_HTTP_CLIENT")
target_link_libraries( aia_demo PRIVATE aiahttpclient )
target_include_directories(aia_demo PUBLIC ${PROJECT_SOURCE_DIR}/ApplicationUtilities/aiahttpclient/include)
endif()

# Organization of Aia demo in folders.
set_property( TARGET aia_demo PROPERTY FOLDER "demos" )
source_group( "" FILES
              main.c
              ${PROJECT_SOURCE_DIR}/ports/include/aia_config.h )

AiaInstall(aia_demo)
