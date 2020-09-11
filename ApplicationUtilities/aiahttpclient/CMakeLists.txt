#
# Setup the AIA HTTP client build.
#

if(AIA_LIBCURL_HTTP_CLIENT)
add_subdirectory("source")

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif()
