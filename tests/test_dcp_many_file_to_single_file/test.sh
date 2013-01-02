#!/bin/bash

##############################################################################
# Description:
#
#   A test to check if dcp will copy many files to a single file.
#
# Expected behavior:
#
#   An error code should be displayed if this action is attempted.
#
# Reminder:
#
#   Lines that echo to the terminal will only be available if DEBUG is enabled
#   in the test runner (test_all.sh).
##############################################################################

# Turn on verbose output
#set -x

# Print out the basic paths we'll be using.
echo "Using dcp binary at: $DCP_TEST_BIN"
echo "Using mpirun binary at: $DCP_MPIRUN_BIN"
echo "Using tmp directory at: $DCP_TEST_TMP"

##############################################################################
# Generate the paths for:
#   * Four files with zero length.
#   * Four files which contain random data.
PATH_A_EMPTY="$DCP_TEST_TMP/dcp_test_many_file_to_single_file.$RANDOM.tmp"
PATH_B_EMPTY="$DCP_TEST_TMP/dcp_test_many_file_to_single_file.$RANDOM.tmp"
PATH_C_EMPTY="$DCP_TEST_TMP/dcp_test_many_file_to_single_file.$RANDOM.tmp"
PATH_D_EMPTY="$DCP_TEST_TMP/dcp_test_many_file_to_single_file.$RANDOM.tmp"
PATH_E_RANDOM="$DCP_TEST_TMP/dcp_test_many_file_single_file.$RANDOM.tmp"
PATH_F_RANDOM="$DCP_TEST_TMP/dcp_test_many_file_to_single_file.$RANDOM.tmp"
PATH_G_RANDOM="$DCP_TEST_TMP/dcp_test_many_file_to_single_file.$RANDOM.tmp"
PATH_H_RANDOM="$DCP_TEST_TMP/dcp_test_many_file_to_single_file.$RANDOM.tmp"

# Print out the generated paths to make debugging easier.
echo "A_EMPTY  path at: $PATH_A_EMPTY"
echo "B_EMPTY  path at: $PATH_B_EMPTY"
echo "C_EMPTY  path at: $PATH_C_EMPTY"
echo "D_EMPTY  path at: $PATH_D_EMPTY"
echo "E_RANDOM path at: $PATH_E_RANDOM"
echo "F_RANDOM path at: $PATH_F_RANDOM"
echo "G_RANDOM path at: $PATH_G_RANDOM"
echo "H_RANDOM path at: $PATH_H_RANDOM"

# Create empty files.
touch $PATH_A_EMPTY
touch $PATH_B_EMPTY
touch $PATH_C_EMPTY
touch $PATH_D_EMPTY

# Create the random files.
dd if=/dev/urandom of=$PATH_E_RANDOM bs=2M count=4
dd if=/dev/urandom of=$PATH_F_RANDOM bs=3M count=3
dd if=/dev/urandom of=$PATH_G_RANDOM bs=1M count=1
dd if=/dev/urandom of=$PATH_H_RANDOM bs=6M count=2

# FIXME: write the rest of this test.
exit 1

##############################################################################
# Test copying the directory to an empty file. The result should be an error.

$DCP_MPIRUN_BIN -np 3 $DCP_TEST_BIN $PATH_A_DIRECTORY $PATH_B_EMPTY
if [[ $? -eq 0 ]]; then
    echo "Unexpectedd success when copying a directory to an empty file. (A -> B)."
    exit 1;
fi

##############################################################################
# Test copying a directory to a random file. The result should be an error.

$DCP_MPIRUN_BIN -np 3 $DCP_TEST_BIN $PATH_A_DIRECTORY $PATH_C_RANDOM
if [[ $? -eq 0 ]]; then
    echo "Unexpected success when copying a directory to a random file (A -> C)."
    exit 1;
fi

##############################################################################
# Since we didn't find any problems, exit with success.

exit 0

# EOF
