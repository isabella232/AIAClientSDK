include(../../cmake/AiaInstall.cmake)

add_library( aiaport INTERFACE )

target_link_libraries( aiaport INTERFACE
                       aiaclockport
                       aiacryptoport
                       aiaiotport
                       aiahttpport
                       aialwaport
                       aiaregistrationport
                       aiastorageport 
                       aiacore )

AiaInstall(aiaport)
