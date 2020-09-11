# aiauxmanager unit test sources.
set( AIAUXMANAGER_UNIT_TEST_SOURCES
     unit/aia_ux_manager_tests.c)

# aiauxmanager tests executable.
add_executable( aia_tests_aiauxmanager
                ${AIAUXMANAGER_UNIT_TEST_SOURCES}
                aiauxmanager_tests.c
                ${AIA_TEST_APP_FILES} )

# Add tests for make test
add_test( NAME "AiaUXManagerTests" COMMAND aia_tests_aiauxmanager )

# Define the test to run.
target_compile_definitions( aia_tests_aiauxmanager PRIVATE
                            -DRunTests=RunAiaUXManagerTests )

# aiauxmanager tests library dependencies.
target_link_libraries( aia_tests_aiauxmanager PRIVATE aiamockregulator aiamockspeakermanager aiauxmanager aiatestutilities unityfixture )

# Organization of aiauxmanager tests in folders.
set_property( TARGET aia_tests_aiauxmanager PROPERTY FOLDER "tests" )
source_group( unit FILES ${AIAUXMANAGER_UNIT_TEST_SOURCES} )
source_group( "" FILES ${AIA_TEST_APP_FILES} aiauxmanager_tests.c )
