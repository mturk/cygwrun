# Building cygwrun

This project contains the source code for cygwrun, a program
that helps to run windows applications under Posix environment.

Cygwrun release comes with the **cygwrun.exe** binary.
In case you wish to create your own binary build,
download or clone cygwrun sources and follow a
few standard rules.

## Prerequisites

To compile cygwrun from source code you will Cygwin with
the following packages included:
  * mingw64-x86_64-binutils
  * mingw64-x86_64-gcc-core
  * mingw64-x86_64-headers
  * mingw64-x86_64-runtime


## Build

Cygwrun release comes with cygwrun.exe binary. However in
case you need to create your own build simply
download cygwrun source release or clone
this repository and follow this simply rules.

Open your Cygwin shell and cd to the Cygwrun source
code directory and type:

```sh
    $ make
```

On success, the cygwrun.exe will be located inside
**.build** directory.

### Makefile targets

Makefile has two additional targets which can be useful
for cygwrun development and maintenance

```sh
    $ make clean
```

This will remove all produced binaries and object files
by simply deleting **.build** subdirectory.

```sh
    $ make test
```

This will compile two test utilities and then
run the `runtest.sh` script.


### Vendor version support

At compile time you can define vendor suffix and/or version
by using the following:

```cmd
> make "_BUILD_VENDOR=_1.acme"
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
