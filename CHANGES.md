# Cygwrun CHANGES

This is a high-level summary of the most important changes.
For a full list of changes, see the [git commit log][log]

  [log]: https://github.com/mturk/cygwrun/commits/


## v1.2.4

 * In development

## v1.2.3

 * Send 'Y' to stdin if PROGRAM is cmd.exe executing batch file
 * Properly kill PROGRAM on timeout
 * Use custom return values on failure
 * Add **-i** command option that can disable stdio pipes

## v1.2.2

 * Add support to run under MSYS2 environment
 * Use CYGWRUN_POSIX_ROOT instead deprecated CYGWIN_ROOT and POSIX_ROOT
 * Set PATH to CYGWRUN_POSIX_PATH if set by user
 * Add **-o** command option that sets PATH to ORIGINAL_PATH
 * Add **-t** command option that sets PROGRAM timeout

## v1.2.1

 * Fix detection of the path separators
 * Set PATH to POSIX_PATH if set by user

## v1.2.0

 * Fix searching for programs having relative paths
 * Enable relative paths with **-w** command line option
 * Rename Makefile to Makefile.mak and Makefile.gmk to Makefile

## v1.1.9

 * Drop useless **-e** command line option
 * Fix parsing environment variables
 * Skip empty entries when parsing variables with multiple paths
 * Fix parsing quoted arguments

## v1.1.8

 * Use WIN32 API for running programs
 * Fix argument quoting

## v1.1.7

 * Make `--` option to behave the same way as single `-`
 * Fix processing arguments for batch files

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
