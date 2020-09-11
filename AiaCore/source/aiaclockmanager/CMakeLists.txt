include(../../../cmake/AiaInstall.cmake)

add_library( aiaclockmanager
             aia_clock_manager.c )
 
target_link_libraries( aiaclockmanager PUBLIC aiacore )

AiaInstall( aiaclockmanager )
