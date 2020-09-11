# Test runner executable.
set( AIA_TEST_APP_FILES
     ${PROJECT_SOURCE_DIR}/tests/aia_tests.c )

add_subdirectory( utils )
add_subdirectory( aiacore )
add_subdirectory( aiaalertmanager )
add_subdirectory( aiaemitter )
add_subdirectory( aiaregulator )
add_subdirectory( aiasequencer )
add_subdirectory( aiaspeakermanager )
add_subdirectory( aiamicrophonemanager )
add_subdirectory( aiaregistrationmanager )
add_subdirectory( aiauxmanager )
add_subdirectory( aiasecretmanager )
add_subdirectory( aiaclockmanager )
add_subdirectory( aiaconnectionmanager )
add_subdirectory( aiadispatcher )
add_subdirectory( aiaexceptionmanager )
