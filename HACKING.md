Contributor Guidelines for dcp
------------------------------

Submitting code
===============
When submitting code to dcp, please fork the project and create a new branch
named to reflect your changes before submitting a new pull request.

To produce a clean changeset, we ask that you perform an interactive rebase
before submitting a pull request to make commits easier to understand.

Clean, documented, and understandable code is required. At a minimum, we ask
that you follow the options listed in astyle.options in the top directory of
the repository. New code should not generate any warnings from newer versions
of clang or gcc.

Command Line Option Handling Design
===================================
A subtle problem in creating a POSIX-like frontend is handling the many
combinations of directories and files that the user may use for input. To keep
things simple, I’m going to ignore everything but simple files and directories.
Block and character devices, local domain sockets, named pipes, and symbolic
links should be handled as well, but will not be covered here.

To give an idea of what we’re dealing with, here’s the usage message which
defines our POSIX-like interface:

````
    usage: dcp [CdfhpRrv] [--] source_file target_file
           dcp [CdfhpRrv] [--] source_file ... target_directory
````

For all input, we need to know what the base name of the directory is that
we’ll be writing files into. We’ll also need the path of the destination and a
list of source paths. There are also a few “impossible” situations, like trying
to copy a directory into a file. We need to prune out these situations and
present a nice error message to the user.

To figure out what needs to be pruned out, first we need to know what we have.
This is a bit tricky because sometimes the destination does not initially exist
based on what the user is trying to do. However, we’re not in the business of
creating new directories, so it’s safe to say that the destination should be a
single file or directory if the source is a single file or directory. However,
sometimes an error condition will pop up when it doesn’t make sense for
multiple source paths to be copied into a file.

Here’s some pseudocode to demonstrate this concept. First, we check to see if
something exists on disk at the destination path. If it does, we remember that
state for later.

````
if(is_file(last_argument) {
    last_argument_is_file = true
    last_argument_is_dir = false
} elif( is_dir(last_argument) {
    last_argument_is_dir = true
    last_argument_is_file = false
````

If the destination path doesn’t exist, we check to see what the source paths
are. If recursion is turned on, we’ll have a file as the destination if the
source is a single file, otherwise, we need the destination to be a directory.

````
} else {
    if(allow_recursion) {
        last_argument_is_file = true
         for(src in srcs) {
             if (is_dir(src)) {
                 last_argument_is_file = false
             }
         }
        if( number_of_source_files != 1 && last_argument_is_file ) {
            error(impossible_condition)
        } else {
             last_argument_is_dir = !last_argument_is_file
        }
    } else {
        last_argument_is_file = true
    }
````

Now that we know what the last argument should be, we can reason about what
the end result should be. This makes it trivial to prune out the impossible
situations. Writing down all potential input combinations yields the following
impossible conditions: copying one or more directories into a file, copying
many files into a file, copying one or more directories and files into a file.
Encountering any of this input will lead to an error condition.

Take note that all of the impossible conditions we’ve listed have the property
of the destination being a file. We can take advantage of this by catching
many of the error conditions in the logic that determines if we’re in the mode
of copying a single file into another file.

````
if( last_argument_is_file ) {
    if( number_of_source_files == 1 && is_file(src_path) ) {
        set_base(basename_of_last_argument)
        push_work_queue(src_path);
    } else {
        error(impossible_condition)
    }
````

Now that we’ve handled everything but copying source files into directories,
we can easily handle the rest of the potential inputs.

````
} elif( last_argument_is_directory ) {
    set_base(last_argument)

    for(src in srcs) {
        push_work_queue(src.path)
    }

} else {
    error(unsupported_filetype);
}
````

So there we have it, an algorithm to handle user input of directories and files
for dcp.

Workload Distribution Core Design
=================================
TODO: Describe libcircle here.

Distributed Recursive Copy Design
=================================
TODO: Describe how libcircle is used here.

Stage Design
============
TODO: Describe the four stages here.

Continuous Integration
======================
If you have trouble building dcp, please check the continuous integration
status at <https://travis-ci.org/hpc/dcp>. The build and all tests must be
passing before comprehensive testing for a release is initiated.
