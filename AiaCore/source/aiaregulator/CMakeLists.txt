include(../../../cmake/AiaInstall.cmake)

add_library( aiaregulator
             aia_regulator_buffer.c
             aia_regulator.c )

target_link_libraries( aiaregulator PUBLIC aiacore )

AiaInstall(aiaregulator)