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
> C:\cmsc-15.0_40\setenv.bat
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
> nmake install _STATIC_MSVCRT=1 PREFIX=C:\some\directory
```

Standard makefile install target that will
copy the executable to the PREFIX location.

This can be useful if you are building cygwrun with
some Continuous build application that need produced
binaries at a specific location for later use.


