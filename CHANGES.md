# Cygwrun CHANGES

This is a high-level summary of the most important changes.
For a full list of changes, see the [git commit log][log]

  [log]: https://github.com/mturk/cygwrun/commits/


## v1.1.6

 * In development
 * Drop `-k` command line option
 * Rename command line options
   renamed `-s` to `-E`
   renamed `-S` to `-A`


## v1.1.5


### New Features
 * Add full support for building with msys2 or cygwin environment
 * Allow batch files to be used as **PROGRAM**
   by using `cmd.exe`
 * Add **-n ENV[,...]** command line option which causes
   that specified `ENV` variables are not translated
