#
# Setup the AIA Sample PortAudio Microphone Recorder build.
#

if(AIA_PORTAUDIO_MICROPHONE)
add_subdirectory("source")

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif()
