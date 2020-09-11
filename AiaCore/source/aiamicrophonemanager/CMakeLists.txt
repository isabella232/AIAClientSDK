include(../../../cmake/AiaInstall.cmake)


add_library( aiamicrophonemanager
             aia_microphone_manager.c )
 
target_link_libraries( aiamicrophonemanager PUBLIC
                       aiaregulator
                       aiacore )

AiaInstall(aiamicrophonemanager)
