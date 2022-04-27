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
 -f        convert all unknown posix absolute paths
 -s        do not translate environment variables.
 -q        do not print errors to stderr.
 -v        print version information and exit.
 -V        print detailed version information and exit.
 -h | -?   print this screen and exit.
 -p        print arguments instead executing PROGRAM.
 -e        print current environment block end exit.
           if defined, only print variables that begin with ARGUMENTS.

```

## Posix root

Posix root is used to replace posix parts with posix environment root
location inside Windows environment.

Use `-r <directory>` command line option to setup the install location
of the current posix subsystem.

In case the `-r <directory>` was not specified, the program will
check the following environment variables:

First it will check `CYGWIN_ROOT` and then `POSIX_ROOT` variable.
If none of them were defined, the `C:\cygwin64` or `C:\cygwin`
will be used, if those directories exists.

Make sure that you provide a correct posix root since it will
be used as prefix to `/usr, /bin, /tmp` constructing an actual
Windows path.


For example, if Cygwin is installed inside `C:\cygwin64` you
can set either environment variable

```sh
    $ export CYGWIN_ROOT=C:/cygwin64
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
    $ cygwrun -e FOO
    $ FOO=C:\cygwin64\usr;C:\cygwin64\sbin;..\dir
```

However in case the environment variable value contains path element that
cannot be translated to windows path, the original value is preserved,
regardless if all other path elements can be translated.

```sh
    $ export FOO=/usr:/sbin:/unknown:../dir
    $ cygwrun -e FOO
    $ FOO=/usr:/sbin:/unknown:../dir
```

The `-f` command line option forces all absolute posix paths
to be converted to it's windows format.

```sh
    $ export FOO=/usr:/sbin:/unknown:../dir
    $ cygwrun -f -e FOO
    $ FOO=C:\cygwin64\usr;C:\cygwin64\sbin;C:\cygwin64\unknown;..\dir
```

Note that some environment variables are allways removed from the
current environment variable list that is passed to child process,
such as `CYGWIN_ROOT` or `PS1`.

The full list of variables that are allways removed is defined
with `removeenv[]` array in [cygwrun.c](cygwrun.c) source file.


## Command line arguments

Arguments passed to program are converted to windows format.
However unlike environment variables, they
cannot have multiple paths.

If argument is not a path, or it starts with `--` followed
by valid variable name and equal `=` character, data after the
equal character will be translated to windows path.
If translation fails, the original argument will be preserved.


```sh
    $ cygwrun -p A=/tmp
    $ A=C:\cygwin64\tmp
```


## License

The code in this repository is licensed under the [Apache-2.0 License](LICENSE.txt).
