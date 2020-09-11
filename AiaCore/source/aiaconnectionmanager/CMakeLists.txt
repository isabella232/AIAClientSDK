include(../../../cmake/AiaInstall.cmake)

add_library( aiaconnectionmanager
             aia_connection_manager.c )

target_link_libraries( aiaconnectionmanager PUBLIC
                       aiacore )

AiaInstall( aiaconnectionmanager )