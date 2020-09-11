# aiacore unit test sources.
set( AIACORE_UNIT_TEST_SOURCES
     capabilities_sender/aia_capabilities_sender_tests.c
     unit/aia_message_tests.c
     unit/aia_json_message_tests.c 
     unit/aia_binary_message_tests.c
     unit/aia_backoff_tests.c
     unit/aia_json_utils_tests.c
     unit/aia_exception_encountered_utils_tests.c
     unit/aia_utils_tests.c
     stream_buffer/stream_buffer_tests.c
     unit/aia_button_command_sender_tests.c)

if( USE_MBEDTLS )
    list(APPEND AIACORE_UNIT_TEST_SOURCES unit/aia_crypto_mbedtls_tests.c unit/aia_random_mbedtls_tests.c)
endif()

# aiacore tests executable.
add_executable( aia_tests_aiacore
                ${AIACORE_UNIT_TEST_SOURCES}
                aiacore_tests.c
                ${AIA_TEST_APP_FILES} )

# Add tests for make test
add_test( NAME "AiaCoreTests" COMMAND aia_tests_aiacore )

# Define the test to run.
target_compile_definitions( aia_tests_aiacore PRIVATE
                            -DRunTests=RunAiaCoreTests )

# aiacore tests library dependencies.
target_link_libraries( aia_tests_aiacore PRIVATE aiacore aiamocksecretmanager aiamocksequencer aiamockregulator aiamockspeakermanager aiatestutilities unityfixture)

# Organization of aiacore tests in folders.
set_property( TARGET aia_tests_aiacore PROPERTY FOLDER "tests" )
source_group( unit FILES ${AIACORE_UNIT_TEST_SOURCES} )
source_group( "" FILES ${AIA_TEST_APP_FILES} aiacore_tests.c )
