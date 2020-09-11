# aiaspeakermanager unit test sources.
set( AIASPEAKERMANAGER_UNIT_TEST_SOURCES
     unit/aia_speaker_manager_tests.c)

# aiaspeakermanager tests executable.
add_executable( aia_tests_aiaspeakermanager
                ${AIASPEAKERMANAGER_UNIT_TEST_SOURCES}
                aiaspeakermanager_tests.c
                ${AIA_TEST_APP_FILES} )

# Add tests for make test
add_test( NAME "AiaSpeakerManagerTests" COMMAND aia_tests_aiaspeakermanager )

# Define the test to run.
target_compile_definitions( aia_tests_aiaspeakermanager PRIVATE
                            -DRunTests=RunAiaSpeakerManagerTests )

# aiaspeakermanager tests library dependencies.
target_link_libraries( aia_tests_aiaspeakermanager PRIVATE aiamocksequencer aiamockregulator aiaspeakermanager aiatestutilities unityfixture )

# Organization of aiaspeakermanager tests in folders.
set_property( TARGET aia_tests_aiaspeakermanager PROPERTY FOLDER "tests" )
source_group( unit FILES ${AIASPEAKERMANAGER_UNIT_TEST_SOURCES} )
source_group( "" FILES ${AIA_TEST_APP_FILES} aiaspeakermanager_tests.c )
