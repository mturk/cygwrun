/**
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _CYGWRUN_H_INCLUDED_
#define _CYGWRUN_H_INCLUDED_

#if defined(_MSC_VER)
/**
 * Disable or reduce the frequency of...
 *   C4100: unreferenced formal parameter
 *   C4244: int to char/short - precision loss
 *   C4702: unreachable code
 */
# pragma warning(disable: 4100 4244 4702)
#endif

/**
 * Version info
 */
#define PROJECT_MAJOR_VERSION   1
#define PROJECT_MINOR_VERSION   2
#define PROJECT_PATCH_VERSION   0
#if defined(_BUILD_NUMBER)
# define PROJECT_MICRO_VERSION  _BUILD_NUMBER
#else
# define PROJECT_MICRO_VERSION  0
#endif
/**
 * Set to zero for non dev versions
 */
#define PROJECT_ISDEV_VERSION   0

/**
 * Helper macros for properly quoting a value as a
 * string in the C preprocessor
 */
#define CPP_TOSTR_HELPER(n)     #n
#define CPP_TOSTR(n)            CPP_TOSTR_HELPER(n)

#define IS_INVALID_HANDLE(_h)   (((_h) == NULL) || ((_h) == INVALID_HANDLE_VALUE))
#define SAFE_CLOSE_HANDLE(_h)                                           \
    if (((_h) != NULL) && ((_h) != INVALID_HANDLE_VALUE))               \
        CloseHandle((_h));                                              \
    (_h) = NULL

#if defined(_VENDOR_SFX)
# define PROJECT_VENDOR_SFX     CPP_TOSTR(_VENDOR_SFX)
#else
# define PROJECT_VENDOR_SFX     ""
#endif

#if PROJECT_ISDEV_VERSION
# define PROJECT_VERSION_DEV    " (dev)"
#else
# define PROJECT_VERSION_DEV    ""
#endif

/**
 * Construct build stamp
 */
#if defined(_MSC_VER)
# define PROJECT_BUILD_CC       "msc " CPP_TOSTR(_MSC_FULL_VER) "."     \
                                CPP_TOSTR(_MSC_BUILD)
#elif defined(__GNUC__)
# define PROJECT_BUILD_CC       "gcc " CPP_TOSTR(__GNUC__) "."          \
                                CPP_TOSTR(__GNUC_MINOR__) "."           \
                                CPP_TOSTR(__GNUC_PATCHLEVEL__)
#else
# define PROJECT_BUILD_CC       "unknown"
#endif
#if defined(_BUILD_TIMESTAMP)
#define PROJECT_BUILD_TSTAMP    "(" CPP_TOSTR(_BUILD_TIMESTAMP) " " PROJECT_BUILD_CC ")"
#else
#define PROJECT_BUILD_TSTAMP    "(" __DATE__ " " __TIME__ " " PROJECT_BUILD_CC ")"
#endif

/**
 * Macro for .rc files using numeric csv representation
 */
#define PROJECT_VERSION_CSV     PROJECT_MAJOR_VERSION,                  \
                                PROJECT_MINOR_VERSION,                  \
                                PROJECT_PATCH_VERSION,                  \
                                PROJECT_MICRO_VERSION

#define PROJECT_VERSION_MIN                                             \
                                CPP_TOSTR(PROJECT_MAJOR_VERSION) "."    \
                                CPP_TOSTR(PROJECT_MINOR_VERSION) "."    \
                                CPP_TOSTR(PROJECT_PATCH_VERSION)        \

#define PROJECT_VERSION_STR                                             \
                                CPP_TOSTR(PROJECT_MAJOR_VERSION) "."    \
                                CPP_TOSTR(PROJECT_MINOR_VERSION) "."    \
                                CPP_TOSTR(PROJECT_PATCH_VERSION) "."    \
                                CPP_TOSTR(PROJECT_MICRO_VERSION)

#define PROJECT_VERSION_TXT                                             \
                                PROJECT_VERSION_MIN                     \
                                PROJECT_VENDOR_SFX                      \
                                PROJECT_VERSION_DEV

#define PROJECT_VERSION_ALL                                             \
                                PROJECT_VERSION_TXT " "                 \
                                PROJECT_BUILD_TSTAMP

#define PROJECT_NAME            "cygwrun"
#define PROJECT_COMPANY_NAME    "Acme Corporation"

#define PROJECT_COPYRIGHT       \
    "Copyright (c) 1964-2023 The Acme Corporation or its "              \
    "licensors, as applicable."

#define PROJECT_DESCRIPTION     \
    "Run windows applications under posix environment"

#define PROJECT_URL             \
    "https://github.com/mturk/cygwrun"

#define PROJECT_LICENSE_SHORT   \
    "Licensed under the Apache-2.0 License"

#define PROJECT_LICENSE         \
  "Licensed under the Apache License, Version 2.0 (the ""License"");\n"         \
  "you may not use this file except in compliance with the License.\n"          \
  "You may obtain a copy of the License at\n\n"                                 \
  "http://www.apache.org/licenses/LICENSE-2.0\n\n"                              \
  "Unless required by applicable law or agreed to in writing, software\n"       \
  "distributed under the License is distributed on an ""AS IS"" BASIS,\n"       \
  "WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.\n"  \
  "See the License for the specific language governing permissions and\n"       \
  "limitations under the License."


#endif /* _CYGWRUN_H_INCLUDED_ */
