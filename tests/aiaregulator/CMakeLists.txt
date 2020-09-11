# aiaregulator unit test sources.
set( AIAREGULATOR_UNIT_TEST_SOURCES
     unit/aia_regulator_buffer_tests.c
     unit/aia_regulator_tests.c )

# aiaregulator tests executable.
add_executable( aia_tests_aiaregulator
                ${AIAREGULATOR_UNIT_TEST_SOURCES}
                aiaregulator_tests.c
                ${AIA_TEST_APP_FILES} )

# Add tests for make test
add_test( NAME "AiaRegulatorTests" COMMAND aia_tests_aiaregulator )

# Define the test to run.
target_compile_definitions( aia_tests_aiaregulator PRIVATE
                            -DRunTests=RunAiaRegulatorTests )

# aiaregulator tests library dependencies.
target_link_libraries( aia_tests_aiaregulator PRIVATE aiaregulator aiatestutilities unityfixture )

# Organization of aiaregulator tests in folders.
set_property( TARGET aia_tests_aiaregulator PROPERTY FOLDER "tests" )
source_group( unit FILES ${AIAREGULATOR_UNIT_TEST_SOURCES} )
source_group( "" FILES ${AIA_TEST_APP_FILES} aiaregulator_tests.c )
