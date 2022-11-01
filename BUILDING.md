# Building cygwrun

This project contains the source code for cygwrun, a program
that helps to run windows applications under Posix environment.

## Prerequisites

To compile cygwrun from source code you will need either
Microsoft C/C++ Compiler from Microsoft Visual Studio 2010
or some later version, or GCC compiler from msys2

The official distribution release is build by using
GCC compiler from [msys2](https://www.msys2.org)

## Build

cygwrun release comes with cygwrun.exe binary. However in
case you need to create your own build simply
download cygwrun source release or clone
this repository and follow this simply rules.

### Build using mingw64

Cygwrun can be built using GCC compiler from msys2.
You will need to install [msys2](https://www.msys2.org)

After installing msys2 open msys2 shell and
install required packages: base-devel and mingw-w64-x86_64-toolchain
if they are not already installed.

For example
```sh
$ pacman --noconfirm -Sy base-devel
$ pacman --noconfirm -Sy mingw-w64-x86_64-toolchain
```

Restart the shell with `-mingw64` parameter or open `mingw64.exe`
terminal and cd to SvcBach source directory and type

```sh

$ make -f Makefile.gmk
```

In case there are no compile errors the cygwrun.exe is located
inside **x64** subdirectory.


### Build using CMSC

Custom Microsoft Compiler Toolkit Compilation from
<https://github.com/mturk/cmsc> is based on compiler
that comes from Windows Driver Kit version 7.1.0

Presuming that you have downloaded and unzip CMSC release
to the root of C drive.

Open command prompt in the directory where you have
downloaded or cloned cygwrun and do the following

```no-highlight
> C:\cmsc-15.0_40\setenv.bat
Using default architecture: x64
Setting build environment for win-x64/0x0601

> nmake

Microsoft (R) Program Maintenance Utility Version 9.00.30729.207
...
```
In case there are no compile errors the cygwrun.exe is located
inside **x64** subdirectory.

### Build using other MSVC versions

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
> nmake test
```

This will compile two test utilities which can
then be used by `runtest.sh` script from Cygwin
environment.


