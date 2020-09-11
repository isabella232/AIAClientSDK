include(../../../cmake/AiaInstall.cmake)

add_library( aiaclient
             aia_client.c )
 
target_link_libraries( aiaclient PUBLIC
                       aiaalertmanager
                       aiacore
                       aiaemitter
                       aiaregulator
                       aiasecretmanager
                       aiasequencer
                       aiaspeakermanager
                       aiamicrophonemanager
                       aiauxmanager
                       aiaclockmanager
                       aiaconnectionmanager
                       aiadispatcher
                       aiaexceptionmanager )

target_include_directories( aiaclient PUBLIC
                "${PROJECT_SOURCE_DIR}/ApplicationUtilities/aiaclient/include" )

AiaInstall(aiaclient)
