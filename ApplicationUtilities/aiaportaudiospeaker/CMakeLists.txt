#
# Setup the AIA Sample PortAudio Speaker build.
#

if(AIA_PORTAUDIO_SPEAKER)
add_subdirectory("source")

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif()
