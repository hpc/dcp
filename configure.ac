AC_INIT([dcp],[0.1],[github-hpc@lanl.gov],[dcp],[http://github.com/hpc/dcp])

# Keep artifacts in directories as much as possible.
AC_CONFIG_AUX_DIR(auxdir)
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE([foreign])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

# Check for compilers.
AC_PROG_CC
AM_PROG_CC_C_O

# Needed for docs
X_AC_DCOPY_DOXYGEN

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
AC_CHECK_FUNC(lseek64, , AC_MSG_ERROR([This OS does not appear to support lseek64(3).]))
AC_CHECK_FUNC(fstat64, , AC_MSG_ERROR([This OS does not appear to support fstat64(3).]))
AC_CHECK_FUNC(stat64, , AC_MSG_ERROR([This OS does not appear to support stat64(3).]))
AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
AC_FUNC_MALLOC

AC_CONFIG_FILES([Makefile              \
                 src/Makefile          \
                 doc/Makefile          \
                 doc/man/Makefile      \
                 doc/man/man1/Makefile \
])
AC_OUTPUT
