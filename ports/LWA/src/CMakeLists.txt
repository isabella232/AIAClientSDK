include(../../../cmake/AiaInstall.cmake)

add_library( aialwaport
             aia_lwa_config.c)

target_include_directories( aialwaport PUBLIC
                            "${PROJECT_SOURCE_DIR}/ports/LWA/include" )

target_link_libraries( aialwaport PUBLIC aiaiotport )

AiaInstall(aialwaport)
