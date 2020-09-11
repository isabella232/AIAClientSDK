include(../../../cmake/AiaInstall.cmake)

add_library( aiaclockport
             aia_clock_config.c)

target_include_directories( aiaclockport PUBLIC
                            "${PROJECT_SOURCE_DIR}/ports/Clock/include" )

target_link_libraries( aiaclockport PUBLIC aiaiotport aiacore )

AiaInstall(aiaclockport)
