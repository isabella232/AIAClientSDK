include(../../../cmake/AiaInstall.cmake)

add_library( aiadispatcher
             aia_dispatcher.c )

target_link_libraries( aiadispatcher PUBLIC
                       aiacore )

AiaInstall( aiadispatcher )