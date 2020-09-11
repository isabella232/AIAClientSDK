include(../../../cmake/AiaInstall.cmake)

add_library( aiasecretmanager
             aia_secret_manager.c )
 
target_link_libraries( aiasecretmanager PUBLIC aiacore )

aiainstall(aiasecretmanager)
