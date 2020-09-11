include(../../../cmake/AiaInstall.cmake)

add_library( aiaportaudiomicrophone
             aia_portaudio_microphone.c )

target_link_libraries( aiaportaudiomicrophone PUBLIC aiacore portaudio )

target_include_directories( aiaportaudiomicrophone PUBLIC
                "${PROJECT_SOURCE_DIR}/ApplicationUtilities/aiaportaudiomicrophone/include" )

AiaInstall(aiaportaudiomicrophone)
