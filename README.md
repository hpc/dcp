# dcp
##### A tool to copy file(s) in parallel on a distributed system.

### IN-DEVELOPMENT WARNING
Note that dcp is in an early stage of development. To determine the current state of dcp, please view the test results at the bottom of the latest build output on <https://travis-ci.org/hpc/dcp>.

### SYNOPSIS
```
dcp [cCdfhpRrUv] [--] source_file target_file
dcp [cCdfhpRrUv] [--] source_file ... target_directory
```

### DESCRIPTION
dcp is a file copy tool in the spirit of *cp(1)* that evenly distributes work across a large cluster without any centralized state. It is designed for copying files which are located on a distributed parallel file system. The method used in the file copy process is a self-stabilization algorithm which enables per-node autonomous processing and a token passing scheme to detect termination.

### PREREQUISITES
An MPI environment is required (such as [Open MPI](http://www.open-mpi.org/)'s *mpirun(1)*) as well as the self-stabilization library known as [LibCircle](https://github.com/hpc/libcircle).

### OPTIONS
**-c**, **--conditional**

When copying a source directory to a destination directory, copy the source directory over the destination directory. The default behavior is to copy the source directory inside the destination directory.

**-C**, **--skip-compare**

Skip the compare operation to confirm file integrity. When using this option, a file integrity check, such as md5sum, should be performed after the file(s) have been copied.

**-d <level>**, **--debug=level**

Specify the level of debug information to output. Level may be one of: *fatal*, *err*, *warn*, *info*, or *dbg*. Increasingly verbose debug levels include the output of less verbose debug levels.

**-f**, **--force**

Remove existing destination files if creation or truncation fails.

**-h**, **--help**

Print a brief message listing the *dcp(1)* options and usage.

**-p**, **--preserve**

Preserve the original files' owner, group, permissions (including the setuid and setgid bits), time of last  modification and time of last access. In case duplication of owner or group fails, the setuid and setgid bits are cleared.

**-R**, **--recursive**

Copy directories recursively, and do the right thing when objects other than ordinary files or directories are encountered.

**-r**, **--recursive-unspecified**

Copy directories recursively, and ignore objects other than ordinary files or directories.

**-U**, **--unreliable-filesystem**

If the filesystem is very unreliable, this option may be used to always retry an operation when a failure occurs. If failures are permanent, this option will cause an infinite loop.

**-v**, **--version**

Print version information and exit.

### RPM CREATION
To create a dcp rpm, simply follow these steps. Replace *version* and
*release* with the appropriate values.

```
git clone https://github.com/hpc/dcp.git dcp-<version>-<release>
tar -zcvf dcp-<version>-<release>.tgz dcp-<version>-<release>
rpmbuild -ta dcp-<version>-<release>.tgz
```

### Contributions
Please view the *HACKING.md* file for more information on how to contribute to dcp.

### COPYING
See the included *COPYING* file for additional information. 
