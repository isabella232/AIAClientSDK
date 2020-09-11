
# Set up Opus codec for the Aia SDK.
#
# If Opus is not already installed in a standard system path, specify its
# location in your CMAKE_PREFIX_PATH or manually define the paths as follows:
#     cmake <path-to-source> 
#           -DLIBOPUS_INCLUDE_DIR=<path-to-opus-include> 
#           -DLIBOPUS_LIB_PATH=<path-to-libopus>

if (NOT LIBOPUS_INCLUDE_DIR OR NOT LIBOPUS_LIB_PATH)
    find_package(PkgConfig)
    pkg_check_modules(OPUS opus IMPORTED_TARGET GLOBAL)
    add_library(opus ALIAS PkgConfig::OPUS)
else()
    add_library(opus INTERFACE IMPORTED GLOBAL)
    target_include_directories( opus INTERFACE "${LIBOPUS_INCLUDE_DIR}" )
    target_link_libraries( opus INTERFACE "${LIBOPUS_LIB_PATH}" )
endif()
