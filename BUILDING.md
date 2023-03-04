# Building cygwrun

This project contains the source code for cygwrun, a program
that helps to run windows applications under Posix environment.

The build process supports only command line tools
for both Microsoft and GCC compilers.

Cygwrun release comes with the **cygwrun.exe** binary.
In case you wish to create your own binary build,
download or clone cygwrun sources and follow a
few standard rules.

## Prerequisites

To compile cygwrun from source code you will need either
Microsoft C/C++ Compiler from Microsoft Visual Studio 2015
or any later version. Alternatively you use
[MSYS2](https://www.msys2.org) mingw64 compiler toolchain.

The official distribution release is build by using
GCC compiler from [msys2](https://www.msys2.org)

## Build

Cygwrun release comes with cygwrun.exe binary. However in
case you need to create your own build simply
download cygwrun source release or clone
this repository and follow this simply rules.

### Build using mingw64

Cygwrun can be built using GCC compiler from msys2.
You will need to install [msys2](https://www.msys2.org)

After installing msys2 open msys2 shell and
install required packages: base-devel and mingw-w64-x86_64-toolchain
if they are not already installed.

For example:
```sh
$ pacman --noconfirm -Sy base-devel
$ pacman --noconfirm -Sy mingw-w64-x86_64-toolchain
```

Restart the shell with `-mingw64` parameter or open `mingw64.exe`
terminal and cd to cygwrun source directory and type

```sh

$ make -f Makefile.gmk
```

In case there are no compile errors the cygwrun.exe is located
inside **x64** subdirectory.


### Build using Visual Studio

To build the cygwrun using an already installed Visual Studio,
you will need to open the Visual Studio native x64 command
line tool.

If using Visual Studio 2017 or later, open command prompt
and call `vcvars64.bat` from Visual Studio install location
eg, `C:\Program Files\Visual Studio 2017\VC\Auxiliary\Build`

```cmd
> cd C:\Some\Location\cygwrun
> nmake

```

The binary should be inside **x64** subdirectory.

Using Visual Studio, cygwrun.exe can be built
as statically linked to the MSVCRT library.

Add `_STATIC_MSVCRT=1` as nmake parameter:
```cmd
> nmake _STATIC_MSVCRT=1

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


### Vendor version support

At compile time you can define vendor suffix and/or version
by using the following:

```cmd
> nmake "_VENDOR_SFX=_1.acme"
```

This will create build with version strings set to `x.y.z_1.acme` where
`x.y.z` are cygwrun version numbers.

## Creating Release

Ensure that each release tag starts with letter **v**,
eg **v0.9.1-dev**, **v1.0.37.alpha.1** or similar.
Check [Semantic Versioning 2.0](https://semver.org/spec/v2.0.0.html)
for more guidelines.

To create a .zip distribution archive download
and extract the 7-zip standalone console version from
[7-Zip Extra](https://www.7-zip.org/a/7z2107-extra.7z)
and put **7za.exe** somewhere in the PATH.

Run the [mkrelease.bat](../mkrelease.bat) or [mkrelease.sh](../mkrelease.sh) script file
to compile and create required metadata and release assets.

Edit **cygwrun-x.y.z-win-x64.txt** and put it's content
in GitHub release description box.

Finally add the **cygwrun-x.y.z-win-x64.zip**
file to the release assets.
