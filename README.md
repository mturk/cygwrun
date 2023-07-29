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
 -r <DIR>  use DIR as posix root.
 -w <DIR>  change working directory to DIR before calling PROGRAM.
 -n <ENV>  do not translate ENV variable(s)
           multiple variables are comma separated.
 -t <SEC>  timeout in SEC (seconds) for PROGRAM to finish.
 -o        use ORIGINAL_PATH instead PATH.
 -f        convert all unknown posix absolute paths.
 -K        keep trailing path separators for paths.
 -q        do not print errors to stderr.
 -v        print version information and exit.
 -V        print detailed version information and exit.
 -h | -?   print this screen and exit.
 -p        print arguments instead executing PROGRAM.

```

## Posix root

Posix root is used to replace posix parts with posix environment root
location inside Windows environment.

Use `-r <directory>` command line option to setup the install location
of the current posix subsystem.

In case the `-r <directory>` was not specified, the program will
check the following environment variables:

First it will check `CYGWRUN_POSIX_ROOT` then `POSIX_ROOT` and
finally `CYGWIN_ROOT` variable.

If none of them were defined, cygwrun will use the first directory
part of the parent process as posix root directory.

Make sure that you provide a correct posix root since it will
be used as prefix to `/usr, /bin, /tmp` constructing an actual
Windows path.

**Notice**

The `POSIX_ROOT` and `CYGWIN_ROOT` environment variables
are deprecated starting with version **1.2.2** and might
be removed in future releases.


For example, if Cygwin is installed inside `C:\cygwin64` you
can set either environment variable

```sh
    $ export CYGWRUN_POSIX_ROOT=C:/cygwin64
    ...
    $ cygwrun ... --f1=/usr/local
```

... or declare it on command line

```sh
    $ cygwrun -r C:/cygwin64 ... --f1=/usr/local
```

In both cases `--f1 parameter` will evaluate to `--f1=C:\cygwin64\usr\local`

## Environment variables

Any Cygwin program that calls Cygwrun, will already translate
most of the known environment variables to Windows format,
but not all of them.

Cygwrun presumes that it's called from some Cygwin process,
and it will try to translate all environment variables from
posix to windows notation.

For example, if environment variable contains valid posix path(s)
it will be translated to windows path(s).

```sh
    $ export FOO=/usr:/sbin:../dir
    $ cygwrun dumpenvp.exe FOO
    $ FOO=C:\cygwin64\usr;C:\cygwin64\sbin;..\dir
```

However in case the environment variable value contains path element that
cannot be translated to windows path, the original value is preserved,
regardless if all other path elements can be translated.

```sh
    $ export FOO=/usr:/sbin:/unknown:../dir
    $ cygwrun dumpenvp.exe FOO
    $ FOO=/usr:/sbin:/unknown:../dir
```

The `-f` command line option forces all absolute posix paths
to be converted to it's windows format.

```sh
    $ export FOO=/usr:/sbin:/unknown:../dir
    $ cygwrun -f dumpenvp.exe FOO
    $ FOO=C:\cygwin64\usr;C:\cygwin64\sbin;C:\cygwin64\unknown;..\dir
```

Note that some environment variables are always removed from the
current environment variable list that is passed to child process
such as `_`, `CYGWRUN_POSIX_PATH`, `OLDPWD`, `ORIGINAL_PATH`,
`ORIGINAL_TEMP` and `ORIGINAL_TMP`.


## Running from MSYS2

If running inside MSYS2 environment, MSYS2 will try to convert
environment variables and program arguments to native windows paths.

This can sometimes produce false paths.
For example if trying to pass `/Q` argument to native application
MSYS2 will actually pass the `Q:\'

```sh
    $ dumpargs.exe /Q
    $ Q:\
```

To handle this you can use the following

```sh
    $ MSYS2_ARG_CONV_EXCL=* dumpargs.exe /Q
    $ /Q
```

However if trying to pass `/Q /tmp` this will not resolve correctly

```sh
    $ dumpargs.exe /Q /tmp
    $ Q:/
    $ C:/msys64/tmp
    ...
    $ MSYS2_ARG_CONV_EXCL=* dumpargs.exe /Q /tmp
    $ /Q
    $ /tmp
```

Use Cygwrun will solve the arguments correctly

```sh
    $ MSYS2_ARG_CONV_EXCL=* cygwrun.exe dumpargs.exe /Q /tmp
    $ /Q
    $ C:\msys64\tmp
```

The same applies to the environment variables.
The best practice when running native applications
inside MSYS2 is to use Cygwrun as following

```sh
    $ MSYS2_ARG_CONV_EXCL=* MSYS2_ENV_CONV_EXCL=* cygwrun.exe myapp.exe [arguments]
```

This will ensure that arguments and environment variables
are properly converted to native Windows paths.


## Command line arguments

All arguments passed to `PROGRAM` are converted to windows format.

If argument is not a path, or it starts with `--`, and then followed
by valid variable name and equal `=` character, data after the
equal character will be translated to windows path.
If translation fails, the original argument will be preserved.


```sh
    $ cygwrun -p A=/tmp
    $ A=C:\cygwin64\tmp
    $
    $ cygwrun -p --A=/tmp
    $ --A=C:\cygwin64\tmp
```

## Managing PATH environment variable

By default Cygwrun will use the **PATH** environment
variable passed by calling process and translate it
to Windows convention.

Since Cygwin or MSYS2 modifies the original PATH, user
can use the **-o** command line option to set the PATH
to the ORIGINAL_PATH. The ORIGINAL_PATH is variable
set by Cygwin which contains the PATH before Cygwin
was started.

This is useful when Windows PATH contains the same
program as Cygwin, like `python.exe`, `find.exe` etc.

The other solution is to set the `CYGWRUN_POSIX_PATH`
environment variable before calling Cygwrun.

This variable will be to find the PROGRAM if defined
as relative path, and will be passed as PATH variable
to the child process.


## License

The code in this repository is licensed under the [Apache-2.0 License](LICENSE.txt).
