include $(top_srcdir)/common.mk

bin_PROGRAMS = dcp
dcp_SOURCES = checksum.c copy.c filestat.c dcp.c
dcp_LDADD = \
    $(libcircle_LIBS) \
    $(MPI_CLDFLAGS)

dcp_CPPFLAGS = \
    $(MPI_CFLAGS)       \
    $(libcircle_CFLAGS)
