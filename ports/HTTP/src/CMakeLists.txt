include(../../../cmake/AiaInstall.cmake)

add_library( aiahttpport
             aia_http_config.c)

target_include_directories( aiahttpport PUBLIC
                            "${PROJECT_SOURCE_DIR}/ports/HTTP/include" )

target_link_libraries( aiahttpport PUBLIC aiahttpclient )

AiaInstall(aiahttpport)
