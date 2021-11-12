# cygwrun

Run windows applications under Posix environment

## Overview

Cygwin uses posix paths and environments which makes most of
the standard windows programs to fail because of path mismatch.
The traditional way of handling that is using Cygwin cygpath
utility which translates Cygwin (posix) paths to their windows
equivalents from shell.

For example a standard usage would be:
```
program.exe "--f1=`cygpath -w /tmp/f1`" "`cygpath -w /usr/f1`" ...
```
This can become very complex and it requires that the shell
script is aware it runs inside the Cygwin environment.

cygwrun utility does that automatically by replacing each posix
argument that contains path element with its windows equivalent.
It also replaces paths in the environment variable values making
sure the multiple path elements are correctly separated using
windows path separator `;`.

Using cygwrun the upper example would become:
```
cygwrun program.exe --f1=/tmp/f1 /usr/f1 ...
```
Before starting `program.exe` cygwrun converts all command line
and environment variables to windows format.

## Usage

Here is what the usage screen displays
```
Usage cygwrun [OPTIONS]... PROGRAM [ARGUMENTS]...
Execute PROGRAM [ARGUMENTS]...

Options are:
 -r <DIR>  use DIR as posix root
 -w <DIR>  change working directory to DIR before calling PROGRAM
 -k        keep extra posix environment variables.
 -p        print arguments instead executing PROGRAM.
 -e        print current environment block end exit.
 -v        print version information and exit.
 -h        print this screen and exit.
```

Command options are case sensitive.

## Posix root

Posix root is used to replace posix parts with posix environment root
location inside Windows environment.

Use `-r <directory>` command line option to setup the install location
of the current posix subsystem.

In case the `-r <directory>` was not specified the program will
check the following environment variables.
First it will check `CYGWIN_ROOT` and then `POSIX_ROOT`. If
none of them are defined, cygwrun will check if its parent process
(usually `bash.exe`) path ends with `usr/local/bin/`, `usr/bin/` or `bin/`
and has `etc/fstab` file inside that directory.
If none are found `%SYSTEMDRIVE%:\cygwin64` will be used as default.

Make sure that you provide a correct posix root since it will
be used as prefix to `/usr, /bin, /tmp` constructing an actual
Windows path.


For example, if Cygwin is installed inside `C:\cygwin64` you
can set either environment variable

```
    $ export CYGWIN_ROOT=C:/cygwin64
    ...
    $ cygwrun ... -f1=/usr/local
```

Or declare it on command line

```
    $ cygwrun -r C:/cygwin64 ... -f1=/usr/local
```

In both cases `--f1 parameter` will evaluate to `C:\cygwin64\usr\local`


## License

The code in this repository is licensed under the [Apache-2.0 License](LICENSE.txt).
