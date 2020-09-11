# aiaalertmanager unit test sources.
set( AIAALERTMANAGER_UNIT_TEST_SOURCES
     unit/aia_alert_manager_tests.c)

# aiaalertmanager tests executable.
add_executable( aia_tests_aiaalertmanager
                ${AIAALERTMANAGER_UNIT_TEST_SOURCES}
                aiaalertmanager_tests.c
                ${AIA_TEST_APP_FILES} )

# Add tests for make test
add_test( NAME "AiaAlertManagerTests" COMMAND aia_tests_aiaalertmanager )

# Define the test to run.
target_compile_definitions( aia_tests_aiaalertmanager PRIVATE
                            -DRunTests=RunAiaAlertManagerTests )

# aiaalertmanager tests library dependencies.
target_link_libraries( aia_tests_aiaalertmanager PRIVATE aiamockregulator aiaalertmanager aiatestutilities unityfixture )

# Organization of aiaalertmanager tests in folders.
set_property( TARGET aia_tests_aiaalertmanager PROPERTY FOLDER "tests" )
source_group( unit FILES ${AIAALERTMANAGER_UNIT_TEST_SOURCES} )
source_group( "" FILES ${AIA_TEST_APP_FILES} aiaalertmanager_tests.c )
