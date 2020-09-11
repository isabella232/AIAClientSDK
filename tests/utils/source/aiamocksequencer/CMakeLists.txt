add_library( aiamocksequencer aia_mock_sequencer.c )
target_link_libraries( aiamocksequencer PUBLIC aiacore )
target_include_directories( aiamocksequencer PUBLIC "${PROJECT_SOURCE_DIR}/tests/utils/include" )
