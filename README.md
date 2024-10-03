# cygwrun

Run Windows applications under Cygwin environment

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

In case the first argument is **-v**, Cygwrun will
print version information and exit.

In case the Cygwrun was compiled with command options
enabled, the command line usage is as follows:

```
cygwrun [OPTIONS]... PROGRAM [ARGUMENTS]...

Options are:

 -s=<ENV>  do not translate <ENV> variable(s)
		   multiple variables are comma separated.
 -u=<ENV>  remove <ENV> variable(s) from the PROGRAM environment
		   multiple variables are comma separated.
 -v        print version information and exit.

```

The command options can be enabled by setting
`#define CYGWRUN_HAVE_CMDOPTS 1` inside
[cygwrun.c](./cygwrun.c) file.


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

If set, the **CYGWRUN_PATH** environment variable will
be used to find the `PROGRAM` if defined as relative path,
and passed as **PATH** variable to the child process.

This is useful when Windows PATH contains the same
program as Cygwin, like `python.exe`, `find.exe` etc.


## Program life cycle

By default Cygwrun will execute `PROGRAM` and wait
until it finishes.

In case `CTRL+C` signal is send to Cygwrun, it will
wait for two seconds and then kill the entire process
tree if the process did not terminate.

In case `CTRL+BREAK` signal is send to Cygwrun, it will
wait for three seconds for process to terminate, and then
send `CTRL+C` signal to the process. If the process did
not terminate within this interval it will be terminated.


## Return value

When the `PROGRAM` finishes, the return value of the
program is returned as Cygwrun exit code.

In case the return value is larger then `110`,
it will be truncated to `110`.

Return values from `111` to `125` are generated
by Cygwrun in case of failure.

Return value `126` is returned in case the `PROGRAM`
cannot be executed.
Return value `127` is returned in case the `PROGRAM`
cannot be found.


## License

The code in this repository is licensed under the [Apache-2.0 License](LICENSE.txt).
