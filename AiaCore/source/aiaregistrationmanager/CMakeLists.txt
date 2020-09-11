include(../../../cmake/AiaInstall.cmake)


add_library( aiaregistrationmanager
             aia_registration_manager.c )

target_link_libraries( aiaregistrationmanager PUBLIC
                       aiacore )

AiaInstall(aiaregistrationmanager)
