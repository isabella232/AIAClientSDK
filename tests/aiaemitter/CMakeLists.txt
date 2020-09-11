# aiaemitter unit test sources.
set( AIAEMITTER_UNIT_TEST_SOURCES
     unit/aia_emitter_tests.c )

# aiaemitter tests executable.
add_executable( aia_tests_aiaemitter
                ${AIAEMITTER_UNIT_TEST_SOURCES}
                aiaemitter_tests.c
                ${AIA_TEST_APP_FILES} )

# Add tests for make test
add_test( NAME "AiaEmitterTests" COMMAND aia_tests_aiaemitter )

# Define the test to run.
target_compile_definitions( aia_tests_aiaemitter PRIVATE
                            -DRunTests=RunAiaEmitterTests )

# aiaemitter tests library dependencies.
target_link_libraries( aia_tests_aiaemitter PRIVATE aiaemitter unityfixture )

# Organization of aiaemitter tests in folders.
set_property( TARGET aia_tests_aiaemitter PROPERTY FOLDER "tests" )
source_group( unit FILES ${AIAEMITTER_UNIT_TEST_SOURCES} )
source_group( "" FILES ${AIA_TEST_APP_FILES} aiaemitter_tests.c )
