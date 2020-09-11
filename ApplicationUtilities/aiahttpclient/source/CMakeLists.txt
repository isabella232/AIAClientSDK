include(../../../cmake/AiaInstall.cmake)

add_library( aiahttpclient
             aia_libcurl.c )

target_link_libraries( aiahttpclient PUBLIC aiacore libcurl )

target_include_directories( aiahttpclient PUBLIC
                "${PROJECT_SOURCE_DIR}/ApplicationUtilities/aiahttpclient/include" )

AiaInstall(aiahttpclient)
