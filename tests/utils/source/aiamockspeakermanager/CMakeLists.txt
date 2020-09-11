add_library( aiamockspeakermanager aia_mock_speaker_manager.c )
target_link_libraries( aiamockspeakermanager PUBLIC aiacore )
target_include_directories( aiamockspeakermanager PUBLIC "${PROJECT_SOURCE_DIR}/tests/utils/include" )
