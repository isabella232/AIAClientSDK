include(../../../cmake/AiaInstall.cmake)


add_library( aiaexceptionmanager
             aia_exception_manager.c )

target_link_libraries( aiaexceptionmanager PUBLIC
                       aiaregulator
                       aiacore )

AiaInstall(aiaexceptionmanager)
