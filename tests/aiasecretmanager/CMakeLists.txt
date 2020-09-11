# aiasecretmanager unit test sources.
set( AIASECRETMANAGER_UNIT_TEST_SOURCES
     unit/aia_secret_manager_tests.c)

# aiasecretmanager tests executable.
add_executable( aia_tests_aiasecretmanager
                ${AIASECRETMANAGER_UNIT_TEST_SOURCES}
                aiasecretmanager_tests.c
                ${AIA_TEST_APP_FILES} )

# Add tests for make test
add_test( NAME "AiaSecretManagerTests" COMMAND aia_tests_aiasecretmanager )

# Define the test to run.
target_compile_definitions( aia_tests_aiasecretmanager PRIVATE
                            -DRunTests=RunAiaSecretManagerTests )

# aiasecretmanager tests library dependencies.
target_link_libraries( aia_tests_aiasecretmanager PRIVATE aiamockregulator aiamockspeakermanager aiasecretmanager aiatestutilities unityfixture )

# Organization of aiasecretmanager tests in folders.
set_property( TARGET aia_tests_aiasecretmanager PROPERTY FOLDER "tests" )
source_group( unit FILES ${AIASECRETMANAGER_UNIT_TEST_SOURCES} )
source_group( "" FILES ${AIA_TEST_APP_FILES} aiasecretmanager_tests.c )
