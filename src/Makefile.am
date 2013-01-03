include $(top_srcdir)/common.mk

bin_PROGRAMS = dcp
dcp_SOURCES = handle_args.c prepare.c stat_file.c copy.c compare.c dcp.c
dcp_LDADD = \
    $(libcircle_LIBS) \
    $(MPI_CLDFLAGS)

dcp_CPPFLAGS = \
    $(MPI_CFLAGS)       \
    $(libcircle_CFLAGS)
