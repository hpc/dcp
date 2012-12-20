#!/bin/bash

##############################################################################
# Description:
#
#   A test to check if dcp will copy a single file to another single file.
#
# Expected behavior:
#
#   The first file must overwrite the second file if the second file exists.
#   If the second file does not exist, it should be created and the contents
#   of the first file should be copied in. If the first file does not exist on
#   disk, an error should be reported.
#
# Reminder:
#
#   Lines that echo to the terminal will only be available if DEBUG is enabled
#   in the test runner (test_all.sh).
##############################################################################

# Turn on verbose output
set -x

# Print out the basic paths we'll be using.
echo "Using dcp binary at: $DCP_TEST_BIN"
echo "Using mpirun binary at: $DCP_MPIRUN_BIN"
echo "Using tmp directory at: $DCP_TEST_TMP"

##############################################################################
# Generate the paths for:
#   * A file that doesn't exist at all.
#   * Two files with zero length.
#   * Two files which contain random data.
PATH_A_NOEXIST="$DCP_TEST_TMP/single_file_to_single_file.$RANDOM.tmp"
PATH_B_EMPTY="$DCP_TEST_TMP/single_file_to_single_file.$RANDOM.tmp"
PATH_C_EMPTY="$DCP_TEST_TMP/single_file_to_single_file.$RANDOM.tmp"
PATH_D_RANDOM="$DCP_TEST_TMP/single_file_to_single_file.$RANDOM.tmp"
PATH_E_RANDOM="$DCP_TEST_TMP/single_file_to_single_file.$RANDOM.tmp"

# Print out the generated paths to make debugging easier.
echo "A_NOEXIST path at: $PATH_A_NOEXIST"
echo "B_EMPTY   path at: $PATH_B_EMPTY"
echo "C_EMPTY   path at: $PATH_C_EMPTY"
echo "D_RANDOM  path at: $PATH_D_RANDOM"
echo "E_RANDOM  path at: $PATH_E_RANDOM"

# Create the two empty files.
touch $PATH_B_EMPTY
touch $PATH_C_EMPTY

# Create the two random files.
dd if=/dev/urandom of=$PATH_D_RANDOM bs=1M count=4 > /dev/null 2>&1
dd if=/dev/urandom of=$PATH_E_RANDOM bs=1M count=3 > /dev/null 2>&1

# Create checksums for files that exist on disk.
MD5_B_EMPTY=$(md5sum -q "$PATH_B_EMPTY")
MD5_C_EMPTY=$(md5sum -q "$PATH_C_EMPTY")
MD5_D_RANDOM=$(md5sum -q "$PATH_D_RANDOM")
MD5_E_RANDOM=$(md5sum -q "$PATH_E_RANDOM")

##############################################################################
# Test copying an empty file to an empty file. The result should be two files
# which remain empty with no error output.

$DCP_MPIRUN_BIN -np 3 $DCP_TEST_BIN $PATH_B_EMPTY $PATH_C_EMPTY > /dev/null 2>&1

if [[ $? -ne 0 ]]; then
    echo "Error returned when copying empty file to empty file."
    exit 1;
fi

if [[ "$MD5_B_EMPTY" != $(md5sum -q "$PATH_B_EMPTY") ]]; then
    echo "MD5 mismatch when copying empty file to empty file (B)."
    exit 1
fi

if [[ "$MD5_C_EMPTY" != $(md5sum -q "$PATH_C_EMPTY") ]]; then
    echo "MD5 mismatch when copying empty file to empty file (C)."
    exit 1
fi

##############################################################################
# Test copying a random file to an empty file. The result should be two files
# which both contain the contents of the first random file.

$DCP_MPIRUN_BIN -np 3 $DCP_TEST_BIN $PATH_D_RANDOM $PATH_B_EMPTY #> /dev/null 2>&1

if [[ $? -ne 0 ]]; then
    echo "Error returned when copying random file to empty file."
    exit 1;
fi

if [[ "$MD5_D_RANDOM" != $(md5sum -q "$PATH_B_EMPTY") ]]; then
    echo "MD5 mismatch when copying random file to empty file (B)."
    exit 1
fi


##############################################################################
# Test copying a random file to another random  file. The result should be two
# files which both contain the contents of the first random file.

$DCP_MPIRUN_BIN -np 3 $DCP_TEST_BIN $PATH_E_RANDOM $PATH_D_RANDOM > /dev/null 2>&1

if [[ $? -ne 0 ]]; then
    echo "Error returned when copying random file to random file."
    exit 1;
fi

if [[ "$MD5_E_RANDOM" != $(md5sum -q "$PATH_D_RANDOM") ]]; then
    echo "MD5 mismatch when copying random file to empty file (B)."
    exit 1
fi

##############################################################################
# Since we didn't find any problems, exit with success.

exit 0

# EOF
