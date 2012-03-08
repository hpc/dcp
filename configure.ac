AC_INIT([dcp],[0.1],[github-hpc@lanl.gov],[dcp],[http://github.com/hpc/dcp])

# Keep artifacts in directories as much as possible.
AC_CONFIG_AUX_DIR(auxdir)
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([-Wall no-define])
AC_CONFIG_HEADERS([config.h])

# Check for compilers.
AC_PROG_CC
AM_PROG_CC_C_O

# Needed for tests
#X_AC_DCP_CHECK

# Check for libcircle
PKG_CHECK_MODULES([libcircle], libcircle)

# Pull in libtool. We need to support ancient autotools, so don't use LT_INIT.
#LT_INIT
AC_PROG_LIBTOOL

# Checks for typedefs, structs, and compiler options.
AC_TYPE_SIZE_T

# Checks for library functs.
AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
AC_FUNC_MALLOC

echo
echo "========================================================"
echo "==          dcp: final build configuration            =="
echo "========================================================"
echo "External Library Support:"
echo "  libcircle ...................................... ???"
echo
echo "Build Options:"
echo "  Unit tests ..................................... ???"
echo "========================================================"
echo

AC_CONFIG_FILES([Makefile              \
                 src/Makefile          \
                 doc/Makefile          \
                 doc/man/Makefile      \
                 doc/man/man1/Makefile \
])
AC_OUTPUT
