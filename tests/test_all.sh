#!/bin/bash

#
# A simple integration test runner for dcp.
#

# If we don't find any tests, just don't run anything.
shopt -s nullglob

# Determine where the test directory is
TESTS_DIR=$(dirname ${BASH_SOURCE[0]})

# Make sure we're in the same directory as the tests.
pushd $TESTS_DIR

echo "# =============================================================================="
echo "# Running ALL tests for DCP."
echo "# =============================================================================="
echo "# Tests started at: $(date)"
echo "# =============================================================================="

TESTS_RUN=0
TESTS_FAILED=0
TESTS_PASSED=0

# Find and run all of the tests.
for TEST in ./*
do
    if [[ -d "$TEST" ]]; then
        $($TEST"/test.sh"); RETVAL=$?;

        if [[ $RETVAL -eq 0 ]]; then
            echo "SUCCESS $(echo "$TEST" | sed 's/[^a-zA-Z0-9_]//g')";
            TESTS_PASSED=`expr $TESTS_PASSED + 1`;
        fi
${1%/}
        if [[ $RETVAL -ne 0 ]]; then
            echo "FAILED $(echo "$TEST" | sed 's/[^a-zA-Z0-9_]//g')";
            TESTS_FAILED=`expr $TESTS_FAILED + 1`;
        fi

        TESTS_RUN=`expr $TESTS_RUN + 1`;
    fi
done

echo "# =============================================================================="
echo "# DCP Test Summary:"
echo "#     Passed:         $TESTS_PASSED"
echo "#     Failed:         $TESTS_FAILED"
echo "# =============================================================================="
echo "#     Tests Run:      $TESTS_RUN"
echo "#     Percent Passed: $(echo "scale=2; ($TESTS_PASSED*100) / $TESTS_RUN" | bc)%"
echo "# =============================================================================="
echo "# Tests ended at: $(date)"
echo "# =============================================================================="

# Return to the original directory where this script was run.
popd > /dev/null

# Return failure if any tests failed.
exit $TESTS_FAILED;

# EOF
