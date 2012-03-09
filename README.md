dcp
===
The purpose of dcp is to perform file copy operations on extremely large file
trees and multi-petabyte files. Files are chunked and copied using a
distributed, asyncronous, and autonomous by node algorithm.

Using a filesystem that benefits from heavy parallel reads and writes is
recommended (Such as Lustre or PanFS). This program is designed for
filesystems with more than one hard disk head.

Usage
-----

```dcp [-hvVP] [-d <level>] <source> ... [<special>:]<destination>```

dcp copies directories and files from *source* to *destination*.

See the included dcp(1) man page for a complete listing and description of
options.
