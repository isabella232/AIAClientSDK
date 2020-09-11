# aiamicrophonemanager unit test sources.
set( AIAMICROPHONEMANAGER_UNIT_TEST_SOURCES
     unit/aia_microphone_manager_tests.c)

# aiamicrophonemanager tests executable.
add_executable( aia_tests_aiamicrophonemanager
                ${AIAMICROPHONEMANAGER_UNIT_TEST_SOURCES}
                aiamicrophonemanager_tests.c
                ${AIA_TEST_APP_FILES} )

# Add tests for make test
add_test( NAME "AiaMicrophoneManagerTests" COMMAND aia_tests_aiamicrophonemanager )

# Define the test to run.
target_compile_definitions( aia_tests_aiamicrophonemanager PRIVATE
                            -DRunTests=RunAiaMicrophoneManagerTests )

# aiamicrophonemanager tests library dependencies.
target_link_libraries( aia_tests_aiamicrophonemanager PRIVATE aiamockregulator aiamicrophonemanager aiatestutilities unityfixture )

# Organization of aiamicrophonemanager tests in folders.
set_property( TARGET aia_tests_aiamicrophonemanager PROPERTY FOLDER "tests" )
source_group( unit FILES ${AIAMICROPHONEMANAGER_UNIT_TEST_SOURCES} )
source_group( "" FILES ${AIA_TEST_APP_FILES} aiamicrophonemanager_tests.c )
