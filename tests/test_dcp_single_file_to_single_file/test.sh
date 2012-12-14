#!/bin/bash

# A test to check if dcp will copy a single file to another single file.
#
# Expected behavior:
#
#   The first file must overwrite the second file if the second file exists.
#   If the second file does not exist, it should be created and the contents
#   of the first file should be copied in. If the first file does not exist on
#   disk, an error should be reported.
#
# Reminder: Lines that echo to the terminal will only be available if DEBUG is
#           enabled in the test runner.

# Print out the basic paths we'll be using.
echo "Using dcp binary at: $DCP_TEST_BIN"
echo "Using tmp directory at: $DCP_TEST_TMP"

# Generate the paths for:
#   * A file that doesn't exist at all.
#   * Two files with zero length.
#   * Two files which contain random data.
PATH_A_NOEXIST="$DCP_TEST_TMP/single_file_to_single_file.$RANDOM.tmp"
PATH_B_EMPTY="$DCP_TEST_TMP/single_file_to_single_file.$RANDOM.tmp"
PATH_C_EMPTY="$DCP_TEST_TMP/single_file_to_single_file.$RANDOM.tmp"
PATH_D_RANDOM="$DCP_TEST_TMP/single_file_to_single_file.$RANDOM.tmp"
PATH_E_RANDOM="$DCP_TEST_TMP/single_file_to_single_file.$RANDOM.tmp"

# Print out the paths to make debugging easier.
echo "A_NOEXIST path at: $PATH_A_NOEXIST"
echo "B_EMPTY   path at: $PATH_B_EMPTY"
echo "C_EMPTY   path at: $PATH_C_EMPTY"
echo "D_RANDOM  path at: $PATH_D_RANDOM"
echo "E_RANDOM  path at: $PATH_E_RANDOM"

#### TODO
#####dd if=/dev/urandom of=a.log bs=1M count=2

# TODO: remove this once the test is completely written.
exit 1

# Since we didn't find any problems, exit with success.
exit 0

# EOF
