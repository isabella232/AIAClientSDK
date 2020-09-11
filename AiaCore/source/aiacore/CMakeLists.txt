include(../../../cmake/AiaInstall.cmake)

set( AiaCore_SOURCES )

list(APPEND AiaCore_SOURCES 
             core.c
             init.c
             aia_backoff.c
             aia_encryption_algorithm.c
             aia_secret_derivation_algorithm.c
             aia_message.c
             aia_json_message.c 
             aia_binary_message.c
             aia_json_utils.c
             aia_exception_encountered_utils.c
             aia_topic.c
             aia_utils.c
             capabilities_sender/aia_capabilities_sender.c
             data_stream_buffer/aia_data_stream_buffer.c
             data_stream_buffer/aia_data_stream_buffer_reader.c
             data_stream_buffer/aia_data_stream_buffer_writer.c
             aia_button_command_sender.c)

if( USE_MBEDTLS )
    list(APPEND AiaCore_SOURCES mbedtls/aia_base64_mbedtls.c aia_mbedtls_threading.c aia_crypto_mbedtls.c aia_random_mbedtls.c)
endif()

add_library( aiacore ${AiaCore_SOURCES} )

set( AiaCore_LIBRARIES )

list(APPEND AiaCore_LIBRARIES
           aiaiotport
           aiacrypto
           aiacryptoport)

target_link_libraries( aiacore PUBLIC ${AiaCore_LIBRARIES} )

target_include_directories( aiacore PUBLIC
                            "${PROJECT_SOURCE_DIR}/AiaCore/include" )

AiaInstall(aiacore)
