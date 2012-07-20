# dcp
##### A tool to copy file(s) in parallel on a distributed system.

### IN-DEVELOPMENT NOTICE
Please note that dcp is ALPHA QUALITY software. It is in a state of development where things are constantly changing and unstable.

### SYNOPSIS
```
dcp [CdfhipRrv] [--] source_file target_file
dcp [CdfhipRrv] [--] source_file ... target_directory
```

### DESCRIPTION
dcp is a file copy tool in the spirit of *cp(1)* that evenly distributes work across a large cluster without any centralized state. It is designed for copying files which are located on a distributed parallel file system. The method used in the file copy process is a self-stabilization algorithm which enables per-node autonomous processing and a token passing scheme to detect termination.

### PREREQUISITES
An MPI environment is required (such as [Open MPI](http://www.open-mpi.org/)'s *mpirun(1)*) as well as the self-stabilization library known as [LibCircle](https://github.com/hpc/libcircle).

### OPTIONS
**-C**, **--skip-compare**

Skip the compare operation to confirm file integrity. When using this option, a file integrity check, such as md5sum, should be performed after the file(s) have been copied.

**-d <level>**, **--debug=level**

Specify the level of debug information to output. Level may be one of: *fatal*, *err*, *warn*, *info*, or *dbg*. Increasingly verbose debug levels include the output of less verbose debug levels.

**-f**, **--force**

Remove existing destination files if creation or truncation fails.

**-h**, **--help**

Print a brief message listing the *dcp(1)* options and usage.

**-i**, **--prompt-overwrite**

Prompt whether to overwrite existing regular destination files.

**-p**, **--preserve**

Preserve the original files' owner, group, permissions (including the setuid and setgid bits), time of last  modification and time of last access. In case duplication of owner or group fails, the setuid and setgid bits are cleared.

**-R**, **--recursive**

Copy directories recursively, and do the right thing when objects other than ordinary files or directories are encountered.

**-r**, **--recursive-unspecified**

Copy directories recursively, and do something unspecified with objects other than ordinary files or directories. This is required for POSIX compliance. *dcp(1)* will perform exactly the same actions for **-R** and **-r**.

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

### COPYING
See the included *COPYING* file for additional information. 
