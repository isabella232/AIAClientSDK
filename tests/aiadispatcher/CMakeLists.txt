# aiadispatcher unit test sources.
set( AIADISPATCHER_UNIT_TEST_SOURCES
     unit/aia_dispatcher_tests.c)

# aiadispatcher tests executable.
add_executable( aia_tests_aiadispatcher
                ${AIADISPATCHER_UNIT_TEST_SOURCES}
                aiadispatcher_tests.c
                ${AIA_TEST_APP_FILES} )

# Add tests for make test
add_test( NAME "AiaDispatcherTests" COMMAND aia_tests_aiadispatcher )

# Define the test to run.
target_compile_definitions( aia_tests_aiadispatcher PRIVATE
                            -DRunTests=RunAiaDispatcherTests )

# aiadispatcher tests library dependencies.
target_link_libraries( aia_tests_aiadispatcher PRIVATE aiadispatcher aiamockconnectionmanager aiamockregulator aiamocksecretmanager aiamockspeakermanager aiamocksequencer aiatestutilities unityfixture )

# Organization of aiadispatcher tests in folders.
set_property( TARGET aia_tests_aiadispatcher PROPERTY FOLDER "tests" )
source_group( unit FILES ${AIADISPATCHER_UNIT_TEST_SOURCES} )
source_group( "" FILES ${AIA_TEST_APP_FILES} aiadispatcher_tests.c )
