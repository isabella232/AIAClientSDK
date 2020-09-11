
# Set up libcurl for the Aia SDK.
#
# If libcurl is not already installed in a standard system path, specify its
# location in your CMAKE_PREFIX_PATH or manually define the paths as follows:
#     cmake <path-to-source> 
#           -DLIBCURL_INCLUDE_DIR=<path-to-libcurl-include> 
#           -DLIBCURL_LIB_PATH=<path-to-libcurl>

if (NOT DLIBCURL_INCLUDE_DIR OR NOT DLIBCURL_LIB_PATH)
    find_package(PkgConfig)
    pkg_check_modules(LIBCURL libcurl IMPORTED_TARGET GLOBAL)
    add_library(libcurl ALIAS PkgConfig::LIBCURL)
else()
    add_library(libcurl INTERFACE IMPORTED GLOBAL)
    target_include_directories( libcurl INTERFACE "${DLIBCURL_INCLUDE_DIR}" )
    target_link_libraries( libcurl INTERFACE "${DLIBCURL_LIB_PATH}" )
endif()
