#
# Set up portaudio for the Aia SDK.
#
# If portaudio is not already installed in a standard system path, specify its
# location in your CMAKE_PREFIX_PATH or manually define the paths as follows:
#     cmake <path-to-source> 
#           -DPORTAUDIO_INCLUDE_DIR=<path-to-portaudio-include> 
#           -DPORTAUDIO_LIB_PATH=<path-to-libportaudio>

if (NOT PORTAUDIO_INCLUDE_DIR OR NOT PORTAUDIO_LIB_PATH)
    find_package(PkgConfig)
    pkg_check_modules(PORTAUDIO portaudio-2.0 IMPORTED_TARGET GLOBAL)
    add_library(portaudio ALIAS PkgConfig::PORTAUDIO)
else()
    add_library(portaudio INTERFACE IMPORTED GLOBAL)
    target_include_directories( portaudio INTERFACE "${PORTAUDIO_INCLUDE_DIR}" )
    target_link_libraries( portaudio INTERFACE "${PORTAUDIO_LIB_PATH}" )
    if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        target_link_libraries(portaudio INTERFACE
            "-framework CoreAudio" 
            "-framework AudioToolbox" 
            "-framework AudioUnit" 
            "-framework CoreServices" 
            "-framework Carbon")
    elseif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
        target_link_libraries(portaudio INTERFACE
            rt m pthread asound)
    endif()
endif()
