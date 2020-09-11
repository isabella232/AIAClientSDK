include(../../../cmake/AiaInstall.cmake)

add_library( aiastorageport
             aia_storage_config.c)

target_include_directories( aiastorageport PUBLIC
                            "${PROJECT_SOURCE_DIR}/ports/Storage/include" )

target_link_libraries( aiastorageport PUBLIC aiacore )

AiaInstall(aiastorageport)
