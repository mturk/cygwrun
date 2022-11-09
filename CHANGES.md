# Cygwrun CHANGES

This is a high-level summary of the most important changes.
For a full list of changes, see the [git commit log][log]

  [log]: https://github.com/mturk/cygwrun/commits/


## v1.1.7

 * In development


## v1.1.6

 * Fix path split for environment variables
 * Cleanup command line options
 * Always trust user provided `-r` parameter
 * Convert arguments with multiple paths the same way as environment variables

## v1.1.5


### New Features
 * Add full support for building with msys2 or cygwin environment
 * Allow batch files to be used as **PROGRAM**
   by using `cmd.exe`
 * Add **-n ENV[,...]** command line option which causes
   that specified `ENV` variables are not translated
