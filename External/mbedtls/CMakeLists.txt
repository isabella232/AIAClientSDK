#
# Set up mbedTLS for the Aia SDK.
#
# If mbedTLS is not already installed in a standard system path, specify its
# location in your CMAKE_PREFIX_PATH or manually define the paths as follows:
#     cmake <path-to-source> 
#           -DMBEDTLS_INCLUDE_DIR=<path-to-mbedtls-include> 
#           -DMBEDCRYPTO_LIB_PATH=<path-to-libmbedcrypto>
#
# IMPORTANT: You need to use a copy of mbedTLS that was built with
#            demos/aia_config_mbedtls.h.  See the notes in the
#            README.md on how to build it.

find_path(MBEDTLS_INCLUDE_DIR mbedtls/ssl.h)
if (NOT MBEDTLS_INCLUDE_DIR)
    message(FATAL_ERROR "Could not find mbedtls headers.")
endif()

find_library(MBEDCRYPTO_LIB_PATH mbedcrypto)
if (NOT MBEDCRYPTO_LIB_PATH)
    message(FATAL_ERROR "Could not find mbedcrypto library.")
endif()

add_library(aiacrypto INTERFACE IMPORTED GLOBAL)
target_link_libraries( aiacrypto INTERFACE "${MBEDCRYPTO_LIB_PATH}" )
target_include_directories( aiacrypto INTERFACE "${MBEDTLS_INCLUDE_DIR}" )
target_compile_definitions( aiacrypto INTERFACE -DMBEDTLS_CONFIG_FILE="aia_config_mbedtls.h" )
