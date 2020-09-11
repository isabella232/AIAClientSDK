include(../../../cmake/AiaInstall.cmake)

add_library( aiauxmanager
             aia_ux_manager.c )
 
target_link_libraries( aiauxmanager PUBLIC
                       aiacore )

AiaInstall(aiauxmanager)
