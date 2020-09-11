add_library( aiamockregulator aia_mock_regulator.c )
target_link_libraries( aiamockregulator PUBLIC aiacore )
target_include_directories( aiamockregulator PUBLIC "${PROJECT_SOURCE_DIR}/tests/utils/include" )
