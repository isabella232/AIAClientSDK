#!/bin/sh

#
# This script is used to generate code coverage reports.
# It cleans up old code coverage reports/.gcda files, runs unit tests
# to generate new .gcda files, uses lcov to collect data from the
# files and then generates a code coverage report with genhtml.
#

# Ingest arguments
LCOV="$1"
GENHTML="$2"
COVERAGE_LOCATION="$3"
COVERAGE_HTML_LOCATION="$4"
PROJECT_BINARY_DIR="$5"
PROJECT_SOURCE_DIR="$6"

echo "LCOV = $LCOV"
echo "GENHTML = $GENHTML"
echo "COVERAGE_LOCATION = $COVERAGE_LOCATION"
echo "COVERAGE_HTML_LOCATION = $COVERAGE_HTML_LOCATION"
echo "PROJECT_BINARY_DIR = $PROJECT_BINARY_DIR"
echo "PROJECT_SOURCE_DIR = $PROJECT_SOURCE_DIR"

# Calculated values
base_info="$COVERAGE_LOCATION/lcov_base.info"
test_info="$COVERAGE_LOCATION/lcov_test.info"
combined_info="$COVERAGE_LOCATION/lcov.info"
coverage_info="$COVERAGE_LOCATION/coverage.info"

# Abort if anything fails
set -e

# Cleanup old code coverage
find $PROJECT_BINARY_DIR -name '*.gcda' -type f -delete
rm -rf "$COVERAGE_LOCATION" "$COVERAGE_HTML_LOCATION"
mkdir -p "$COVERAGE_LOCATION"
mkdir -p "$COVERAGE_HTML_LOCATION"

# Capture baseline
echo '==> Running lcov to capture zeroed-out GCOV counters'
"$LCOV" --capture \
        --no-external \
        --initial \
        --ignore-errors=source \
        --directory "$PROJECT_BINARY_DIR" \
        --base-directory "$PROJECT_SOURCE_DIR" \
        --rc lcov_branch_coverage=1 \
        --output-file "$base_info" 2>&1 \
          | grep -v 'ignoring data for external file' \
          | grep -v '^Cannot open source file'

# Run unit tests with make test to generate .gcda files
make test

# Capture tests
echo '==> Running lcov to capture GCOV counters from *.gcda files'
"$LCOV" --capture \
        --no-external \
        --ignore-errors=source \
        --directory "$PROJECT_BINARY_DIR" \
        --base-directory "$PROJECT_SOURCE_DIR" \
        --rc lcov_branch_coverage=1 \
        --output-file "$test_info" 2>&1 \
          | grep -v 'ignoring data for external file' \
          | grep -v '^Cannot open source file'
echo '==> Merging baseline and captured test data'
time "$LCOV" --add "$base_info" \
        --add "$test_info" \
        --rc lcov_branch_coverage=1 \
        --output-file "$combined_info"
echo '==> Filtering test data'
set -x
time "$LCOV" --remove "$combined_info" \
        '*demo*'  \
        '*ports*' \
        '*tests*' \
        --rc lcov_branch_coverage=1 \
        --output-file "$coverage_info" \
          | grep -v '^Removing '
set +x
echo '==> Generating HTML output for developers'
"$GENHTML" -o "$COVERAGE_HTML_LOCATION" \
           --prefix "$PROJECT_SOURCE_DIR" \
           --ignore-errors=source \
           --branch-coverage \
           "$coverage_info"
