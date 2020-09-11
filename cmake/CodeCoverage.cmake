#
# Setup flags and target necessary to run Code Coverage.
#
# To enable code coverage, include the following option on the cmake command line:
#     -DAIA_CODE_COVERAGE=ON
#

option(AIA_CODE_COVERAGE "Enable code coverage." OFF)

if(AIA_CODE_COVERAGE)
  if ( CMAKE_C_COMPILER_ID MATCHES "Clang" OR CMAKE_C_COMPILER_ID MATCHES "GNU" )
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-arcs -ftest-coverage")

    message(STATUS "Looking for lcov")
    find_program(LCOV_PATH lcov)
    message(STATUS "Looking for lcov: ${LCOV_PATH}")

    message(STATUS "Looking for genhtml")
    find_program(GENHTML_PATH genhtml)
    message(STATUS "Looking for genhtml: ${GENHTML_PATH}")

    set(COVERAGE_LOCATION ${PROJECT_BINARY_DIR}/coverage)
    set(COVERAGE_HTML_LOCATION ${PROJECT_BINARY_DIR}/coverage/html)

    add_custom_target(coverage
      COMMENT "Checking unit test line coverage"
      USES_TERMINAL
      COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/cmake/test_coverage.sh"
              "${LCOV_PATH}"
              "${GENHTML_PATH}"
              "${COVERAGE_LOCATION}"
              "${COVERAGE_HTML_LOCATION}"
              "${PROJECT_BINARY_DIR}"
              "${PROJECT_SOURCE_DIR}"
      COMMAND echo HTML Report: ${COVERAGE_HTML_LOCATION}/index.html
      VERBATIM
      WORKING_DIRECTORY ${PROJECT_BINARY_DIR})
  else()
    message(STATUS "Code coverage not supported for compiler")
  endif()
else()
  message(STATUS "Code coverage is DISABLED")
endif()