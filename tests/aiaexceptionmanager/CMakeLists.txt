# aiaexceptionmanager unit test sources.
set( AIAEXCEPTIONMANAGER_UNIT_TEST_SOURCES
     unit/aia_exception_manager_tests.c)

# aiaexceptionmanager tests executable.
add_executable( aia_tests_aiaexceptionmanager
                ${AIAEXCEPTIONMANAGER_UNIT_TEST_SOURCES}
                aiaexceptionmanager_tests.c
                ${AIA_TEST_APP_FILES} )

# Add tests for make test
add_test( NAME "AiaExceptionManagerTests" COMMAND aia_tests_aiaexceptionmanager )

# Define the test to run.
target_compile_definitions( aia_tests_aiaexceptionmanager PRIVATE
                            -DRunTests=RunAiaExceptionManagerTests )

# aiaexceptionmanager tests library dependencies.
target_link_libraries( aia_tests_aiaexceptionmanager PRIVATE aiamockregulator aiaexceptionmanager aiatestutilities unityfixture )

# Organization of aiaexceptionmanager tests in folders.
set_property( TARGET aia_tests_aiaexceptionmanager PROPERTY FOLDER "tests" )
source_group( unit FILES ${AIAEXCEPTIONMANAGER_UNIT_TEST_SOURCES} )
source_group( "" FILES ${AIA_TEST_APP_FILES} aiaexceptionmanager_tests.c )
