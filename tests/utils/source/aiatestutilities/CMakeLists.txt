add_library( aiatestutilities aia_test_utilities.c )
target_link_libraries( aiatestutilities PUBLIC aiacore unityfixture )
target_include_directories( aiatestutilities PUBLIC "${PROJECT_SOURCE_DIR}/tests/utils/include" )
