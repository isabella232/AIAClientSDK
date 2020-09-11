add_library( aiamockconnectionmanager aia_mock_connection_manager.c )
target_link_libraries( aiamockconnectionmanager PUBLIC aiacore )
target_include_directories( aiamockconnectionmanager PUBLIC "${PROJECT_SOURCE_DIR}/tests/utils/include" )
