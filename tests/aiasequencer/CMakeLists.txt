# aiasequencer unit test sources.
set( AIASEQUENCER_UNIT_TEST_SOURCES
     unit/aia_sequencer_tests.c)

# aiasequencer tests executable.
add_executable( aia_tests_aiasequencer
                ${AIASEQUENCER_UNIT_TEST_SOURCES}
                aiasequencer_tests.c
                ${AIA_TEST_APP_FILES} )

# Add tests for make test
add_test( NAME "AiaSequencerTests" COMMAND aia_tests_aiasequencer )

# Define the test to run.
target_compile_definitions( aia_tests_aiasequencer PRIVATE
                            -DRunTests=RunAiaSequencerTests )

# aiasequencer tests library dependencies.
target_link_libraries( aia_tests_aiasequencer PRIVATE aiasequencer unityfixture )

# Organization of aiasequencer tests in folders.
set_property( TARGET aia_tests_aiasequencer PROPERTY FOLDER "tests" )
source_group( unit FILES ${AIASEQUENCER_UNIT_TEST_SOURCES} )
source_group( "" FILES ${AIA_TEST_APP_FILES} aiasequencer_tests.c )
