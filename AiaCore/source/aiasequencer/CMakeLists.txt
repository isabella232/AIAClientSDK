include(../../../cmake/AiaInstall.cmake)

add_library( aiasequencer
             aia_sequencer.c 
             aia_sequencer_buffer.c )
 
target_link_libraries( aiasequencer PUBLIC aiacore )

AiaInstall(aiasequencer)
