include(../../../cmake/AiaInstall.cmake)

add_library( aiaalertmanager
             aia_alert_manager.c )

target_link_libraries( aiaalertmanager PUBLIC
                       aiaregulator
                       aiacore )

AiaInstall(aiaalertmanager)
