#
# Setup the AIA Opus Decoder build.
#

if(AIA_OPUS_DECODER)
add_subdirectory("source")

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif()
