include(../../../cmake/AiaInstall.cmake)

add_library( aiaspeakermanager
             aia_speaker_manager.c )
 
target_link_libraries( aiaspeakermanager PUBLIC
                       aiaregulator
                       aiasequencer
                       aiacore
                       aiaport )

AiaInstall(aiaspeakermanager)
