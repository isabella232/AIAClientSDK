include(../../../cmake/AiaInstall.cmake)

add_library( aiacryptoport
             aia_crypto_config.c)

target_include_directories( aiacryptoport PUBLIC
                            "${PROJECT_SOURCE_DIR}/ports/Crypto/include" )

target_link_libraries( aiacryptoport PUBLIC aiacore )

AiaInstall(aiacryptoport)
