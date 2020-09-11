include(../../../cmake/AiaInstall.cmake)

add_library( aiaopusdecoder
             aia_opus_decoder.c )

target_link_libraries( aiaopusdecoder PUBLIC aiacore opus )

target_include_directories( aiaopusdecoder PUBLIC
                "${PROJECT_SOURCE_DIR}/ApplicationUtilities/aiaopusdecoder/include" )

AiaInstall(aiaopusdecoder)
