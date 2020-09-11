include(../../../cmake/AiaInstall.cmake)

add_library( aiaemitter
             aia_emitter.c )

target_link_libraries( aiaemitter PUBLIC
                       aiacore )

AiaInstall(aiaemitter)
