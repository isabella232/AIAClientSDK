# aiaclockmanager unit test sources.
set( AIACLOCKMANAGER_UNIT_TEST_SOURCES
     unit/aia_clock_manager_tests.c)

# aiaclockmanager tests executable.
add_executable( aia_tests_aiaclockmanager
                ${AIACLOCKMANAGER_UNIT_TEST_SOURCES}
                aiaclockmanager_tests.c
                ${AIA_TEST_APP_FILES} )

# Add tests for make test
add_test( NAME "AiaClockManagerTests" COMMAND aia_tests_aiaclockmanager )

# Define the test to run.
target_compile_definitions( aia_tests_aiaclockmanager PRIVATE
                            -DRunTests=RunAiaClockManagerTests )

# aiaclockmanager tests library dependencies.
target_link_libraries( aia_tests_aiaclockmanager PRIVATE aiamockregulator aiaclockmanager aiatestutilities unityfixture )

# Organization of aiaclockmanager tests in folders.
set_property( TARGET aia_tests_aiaclockmanager PROPERTY FOLDER "tests" )
source_group( unit FILES ${AIACLOCKMANAGER_UNIT_TEST_SOURCES} )
source_group( "" FILES ${AIA_TEST_APP_FILES} aiaclockmanager_tests.c )
