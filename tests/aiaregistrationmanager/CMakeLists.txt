# aiaregistrationmanager unit test sources.
set( AIAREGISTRATIONMANAGER_UNIT_TEST_SOURCES
     unit/aia_registration_manager_tests.c)

# aiaregistrationmanager tests executable.
add_executable( aia_tests_aiaregistrationmanager
                ${AIAREGISTRATIONMANAGER_UNIT_TEST_SOURCES}
                aiaregistrationmanager_tests.c
                ${AIA_TEST_APP_FILES} )

# Add tests for make test
add_test( NAME "AiaRegistrationManagerTests" COMMAND aia_tests_aiaregistrationmanager )

# Define the test to run.
target_compile_definitions( aia_tests_aiaregistrationmanager PRIVATE
                            -DRunTests=RunAiaRegistrationManagerTests )

# aiaregistrationmanager tests library dependencies.
target_link_libraries( aia_tests_aiaregistrationmanager PRIVATE aiaregistrationmanager aiatestutilities unityfixture )

# Organization of aiaregistrationmanager tests in folders.
set_property( TARGET aia_tests_aiaregistrationmanager PROPERTY FOLDER "tests" )
source_group( unit FILES ${AIAREGISTRATIONMANAGER_UNIT_TEST_SOURCES} )
source_group( "" FILES ${AIA_TEST_APP_FILES} aiaregistrationmanager_tests.c )
