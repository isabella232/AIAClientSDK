include(../../../cmake/AiaInstall.cmake)

add_library( aiaregistrationport
             aia_registration_config.c)

target_include_directories( aiaregistrationport PUBLIC
                            "${PROJECT_SOURCE_DIR}/ports/Registration/include"
                            "${PROJECT_SOURCE_DIR}/AiaCore/include" )

target_link_libraries( aiaregistrationport PUBLIC aiaiotport )

AiaInstall(aiaregistrationport)
