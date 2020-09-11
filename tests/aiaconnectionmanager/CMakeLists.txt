# aiaconnectionmanager unit test sources.
set( AIACONNECTIONMANAGER_UNIT_TEST_SOURCES
     unit/aia_connection_manager_tests.c)

# aiaconnectionmanager tests executable.
add_executable( aia_tests_aiaconnectionmanager
                ${AIACONNECTIONMANAGER_UNIT_TEST_SOURCES}
                aiaconnectionmanager_tests.c
                ${AIA_TEST_APP_FILES} )

# Add tests for make test
add_test( NAME "AiaConnectionManagerTests" COMMAND aia_tests_aiaconnectionmanager )

# Define the test to run.
target_compile_definitions( aia_tests_aiaconnectionmanager PRIVATE
                            -DRunTests=RunAiaConnectionManagerTests )

# aiaconnectionmanager tests library dependencies.
target_link_libraries( aia_tests_aiaconnectionmanager PRIVATE aiaconnectionmanager aiatestutilities unityfixture )

# Organization of aiaconnectionmanager tests in folders.
set_property( TARGET aia_tests_aiaconnectionmanager PROPERTY FOLDER "tests" )
source_group( unit FILES ${AIACONNECTIONMANAGER_UNIT_TEST_SOURCES} )
source_group( "" FILES ${AIA_TEST_APP_FILES} aiaconnectionmanager_tests.c )
