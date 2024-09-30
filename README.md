# cygwrun

Run windows applications under Cygwin environment

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

Cygwrun expects that the first argument is
`PROGRAM` name to execute, followed by optional `ARGUMENTS`.

In case the `PROGRAM` name is `.` character, the
Cygwrun will print the evaluated arguments to the
stdout.

The command line usage is as follows:

```
cygwrun [OPTIONS]... PROGRAM [ARGUMENTS]...

Options are:

 -s=<ENV>  do not translate <ENV> variable(s)
		   multiple variables are comma separated.
 -u=<ENV>  remove <ENV> variable(s) from the PROGRAM environment
		   multiple variables are comma separated.
 -v        print version information and exit.
 -V        print detailed version information and exit.

```


## Global configuration

Gygwrun configuration can be modified by setting
environment variables or by using command line options.

Here is the list of environment variables that
Cygwrun uses for each instance:

* **CYGWRUN_PATH**

  If set the value of this variable will be used
  as `PATH` environment variable.

* **CYGWRUN_SKIP**

  This variable allows to set which environment variables
  Cygwrun will not modify.

  The names of the environment variables are comma separated.


* **CYGWRUN_UNSET**

  This variable allows to set which environment variables
  Cygwrun will not pass to the `PROGRAM`.

  The names of the environment variables are comma separated.


## Posix root

Posix root is used to replace posix parts with posix environment root
location inside Windows environment.

On startup cygwrun will try to locate the `cygwrun1.dll`
library and use its parent directory as root.

In case the root directory cannot be found, the program will fail.


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

Note that some environment variables are always removed from the
current environment variable list that is passed to child process
such as `_`, global configuration variables, and any variables
listed inside `CYGWRUN_UNSET` environment variable.



## Command line arguments

All arguments passed to `PROGRAM` are converted to windows format.

If argument is not a path, or it starts with `-` or `--`, and
then followed by valid variable name and equal `=` character,
data after the equal character will be translated to windows path.
If translation fails, the original argument will be preserved.


```sh
    $ cygwrun . A=/tmp
    $ A=C:\cygwin64\tmp
    $
    $ cygwrun . --A=/tmp
    $ --A=C:\cygwin64\tmp
```

## Managing PATH environment variable

By default Cygwrun will use the **PATH** environment
variable passed by calling process and translate it
to Windows convention.

Since Cygwin modifies the original PATH, user
can set the `CYGWRUN_PATH` to the ORIGINAL_PATH.
The ORIGINAL_PATH is variable set by Cygwin which
contains the PATH before Cygwin was started.

This is useful when Windows PATH contains the same
program as Cygwin, like `python.exe`, `find.exe` etc.

The `CYGWRUN_PATH` environment variable will be used to
find the PROGRAM if defined as relative path,
and will be passed as PATH variable to the child process.


## Program life cycle

By default Cygwrun will execute `PROGRAM` and wait
until it finishes.

To set the timeout Cygwrun will wait for program to finish,
use the **^<timeout>** command option or set the
**CYGWRUN_TIMEOUT** environment variable.


```sh
    $
    $ cygwrun ^20 some_program.exe
    $ ...
    $ echo $?
    $ 2
```

In case the `some_program.exe` does not finish within
`20` seconds, cygwrun will first try to send `CTRL+C`
signal to the `some_program.exe`. If the  program
exits within one second the Cygwrun will
return `128+SIGINT (130)` as exit code.

In other case Cygwrun will terminate the `some_program.exe`
and return `128+SIGTERM (143)` error.


** Notice **

The valid range for **timeout** value is between `2`
and `2000000` seconds.

If specified on the command line it is used instead
global value defined by **CYGWRUN_TIMEOUT** environment
variable.

The **timeout** value zero disables timeout.


## Return value

When the `PROGRAM` finishes, the return value of the
program is returned as Cygwrun exit code.

In case the return value is larger then `125`,
it will be truncated to `125`.


## License

The code in this repository is licensed under the [Apache-2.0 License](LICENSE.txt).
