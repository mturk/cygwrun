# Building cygwrun

This project contains the source code for cygwrun, a program
that helps to run windows applications under Posix environment.

## Prerequisites

To compile cygwrun from source code you will need
Microsoft C/C++ Compiler from Microsoft Visual Studio 2010
or some later version.

The official distribution release is build by using
Custom Microsoft Compiler Toolkit Compilation from
<https://github.com/mturk/cmsc> which is based on compiler
that comes from Windows Driver Kit version 7.1.0

cygwrun uses Microsoft compiler, and command line tools to
produce the binary for Windows 64-bit version. The reason is
to simplify the source code and ensure portability across future
Visual Studio versions.

## Build

cygwrun release comes with cygwrun.exe binary. However in
case you need to create your own build simply
download cygwrun source release or clone
this repository and follow this simply rules.

### Build using CMSC

Presuming that you have downloaded and unzip CMSC release
to the root of C drive.

Open command prompt in the directory where you have
downloaded or cloned cygwrun and do the following

```no-highlight
> C:\cmsc-15.0_32\setenv.bat
Using default architecture: x64
Setting build environment for win-x64/0x0601

> nmake

Microsoft (R) Program Maintenance Utility Version 9.00.30729.207
...
```
In case there are no compile errors the cygwrun.exe is located
inside **x64** subdirectory.

### Build using other MSC versions

To build the cygwrun using Visual Studio you already
have on your box you will need to open the Visual Studio
native x64 command line tool. The rest is almost the same

Here is the example for Visual Studio 2012 (others are similar)

Inside Start menu select Microsoft Visual Studio 2012 then
click on Visual Studio Tools and click on
Open VC2012 x64 Native Tools Command Prompt, and then

```no-highlight
> cd C:\Some\Location\cygwrun
> nmake

Microsoft (R) Program Maintenance Utility Version 11.00.50727.1
...
```

### Makefile targets

Makefile has two additional targets which can be useful
for cygwrun development and maintenance

```no-highlight
> nmake clean
```

This will remove all produced binaries and object files
by simply deleting **x64** subdirectory.

```no-highlight
> nmake install PREFIX=C:\some\directory
```

Standard makefile install target that will
copy the executable to the PREFIX location.

This can be useful if you are building cygwrun with
some Continuous build application that need produced
binaries at a specific location for later use.

### Debug compile option

Posix2wx can be compiled to have additional debug
command line option which when specified displays various
internal options at runtime.

When specified this option prints replaced arguments
and environment instead executing PROGRAM.

To compile posix2wix with this option enabled
use the following:

```no-highlight
> nmake install EXTRA_CFLAGS=-D_HAVE_DEBUG_OPTION
```

### Test compile option

For test suite purposed use the following flags:

```no-highlight
> nmake install EXTRA_CFLAGS=-D_TEST_MODE
```

This compiles cygwrun in such a way that instead
PROGRAM you need to specify either `arg` or `env`.

On execution cygwrun will display either arguments
or environment variables and exit. This option is used
for test purposes to verify the produced path translation.

For example open cygwin shell and type the following:

```no-highlight
$ ./cygwrun.exe -r C:/posixroot arg /bin/:/foo:/:/tmp /c/usr/include
C:\posixroot\bin\;/foo:C:\posixroot;C:\posixroot\tmp
C:\usr\include

```

The argument for `env` can be used to filter environment
variables by checking if the environment variable starts
with argument.

```no-highlight
$ export FOO_BAR=/usr/local

$ ./cygwrun.exe -r c:/posixroot env Foo_Bar
FOO_BAR=C:\posixroot\usr\local

```


