dcp (distributed copy)
========================

WARNING: dcp is NOT production quality. It WILL randomly delete files and
hurt small adorable animals (really cute ones!). Please only execute it on
systems that you don't care about or you'll regret it!

The purpose of dcp is to perform a large number of file copy operations (such
as a directory tree with billions of files) across many different nodes in a
cluster. Using a filesystem that benefits from heavy parallel reads and writes
is recommended.

Usage
-----

```dcp -s <source> -d <destination> [-l <debug>]```

dcp copies a file tree from *source* to *destination*. *debug* may be an
integer ranging from 1-5 which results in an increasing level of debug output.
