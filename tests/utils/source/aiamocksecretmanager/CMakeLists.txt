add_library( aiamocksecretmanager aia_mock_secret_manager.c )
target_link_libraries( aiamocksecretmanager PUBLIC aiacore )
target_include_directories( aiamocksecretmanager PUBLIC "${PROJECT_SOURCE_DIR}/tests/utils/include" )
