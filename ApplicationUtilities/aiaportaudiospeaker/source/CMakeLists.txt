include(../../../cmake/AiaInstall.cmake)


add_library( aiaportaudiospeaker
             aia_portaudio_speaker.c )

target_link_libraries( aiaportaudiospeaker PUBLIC aiacore portaudio )

target_include_directories( aiaportaudiospeaker PUBLIC
                "${PROJECT_SOURCE_DIR}/ApplicationUtilities/aiaportaudiospeaker/include" )

AiaInstall(aiaportaudiospeaker)
