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
#define CYGWRUN_MAJOR_VERSION   2
#define CYGWRUN_MINOR_VERSION   0
#define CYGWRUN_PATCH_VERSION   0
#if defined(_BUILD_NUMBER)
# define CYGWRUN_MICRO_VERSION  _BUILD_NUMBER
#else
# define CYGWRUN_MICRO_VERSION  0
#endif
/**
 * Set to zero for non dev versions
 */
#define CYGWRUN_ISDEV_VERSION   0

/**
 * Helper macros for properly quoting a value as a
 * string in the C preprocessor
 */
#define CWR_TOSTR_HELPER(n)     #n
#define CWR_TOSTR(n)            CWR_TOSTR_HELPER(n)

#if defined(_BUILD_VENDOR)
# define CYGWRUN_VENDOR_SFX     CWR_TOSTR(_BUILD_VENDOR)
#else
# define CYGWRUN_VENDOR_SFX     ""
#endif

#if CYGWRUN_ISDEV_VERSION
# define CYGWRUN_VERSION_DEV    " (dev)"
#else
# define CYGWRUN_VERSION_DEV    ""
#endif

/**
 * Construct build stamp
 */
#if defined(_MSC_VER)
# define CYGWRUN_BUILD_CC       "msc " CWR_TOSTR(_MSC_FULL_VER) "."     \
                                CWR_TOSTR(_MSC_BUILD)
#elif defined(__GNUC__)
# define CYGWRUN_BUILD_CC       "gcc " CWR_TOSTR(__GNUC__) "."          \
                                CWR_TOSTR(__GNUC_MINOR__) "."           \
                                CWR_TOSTR(__GNUC_PATCHLEVEL__)
#else
# define CYGWRUN_BUILD_CC       "unknown"
#endif
#if defined(_BUILD_TIMESTAMP)
#define CYGWRUN_BUILD_TSTAMP    "(" CWR_TOSTR(_BUILD_TIMESTAMP) " " CYGWRUN_BUILD_CC ")"
#else
#define CYGWRUN_BUILD_TSTAMP    "(" __DATE__ " " __TIME__ " " CYGWRUN_BUILD_CC ")"
#endif

/**
 * Macro for .rc files using numeric csv representation
 */
#define CYGWRUN_VERSION_CSV     CYGWRUN_MAJOR_VERSION,                  \
                                CYGWRUN_MINOR_VERSION,                  \
                                CYGWRUN_PATCH_VERSION,                  \
                                CYGWRUN_MICRO_VERSION

#define CYGWRUN_VERSION_MIN                                             \
                                CWR_TOSTR(CYGWRUN_MAJOR_VERSION) "."    \
                                CWR_TOSTR(CYGWRUN_MINOR_VERSION) "."    \
                                CWR_TOSTR(CYGWRUN_PATCH_VERSION)        \

#define CYGWRUN_VERSION_STR                                             \
                                CWR_TOSTR(CYGWRUN_MAJOR_VERSION) "."    \
                                CWR_TOSTR(CYGWRUN_MINOR_VERSION) "."    \
                                CWR_TOSTR(CYGWRUN_PATCH_VERSION) "."    \
                                CWR_TOSTR(CYGWRUN_MICRO_VERSION)

#define CYGWRUN_VERSION_TXT                                             \
                                CYGWRUN_VERSION_MIN                     \
                                CYGWRUN_VENDOR_SFX                      \
                                CYGWRUN_VERSION_DEV

#define CYGWRUN_VERSION_ALL                                             \
                                CYGWRUN_VERSION_TXT " "                 \
                                CYGWRUN_BUILD_TSTAMP

#define CYGWRUN_NAME            "cygwrun"
#define CYGWRUN_COMPANY_NAME    "Acme Corporation"

#define CYGWRUN_COPYRIGHT       \
    "Copyright (c) 1964-2024 The Acme Corporation or its "              \
    "licensors, as applicable."

#define CYGWRUN_DESCRIPTION     \
    "Run windows applications under posix environment"

#define CYGWRUN_URL             \
    "https://github.com/mturk/cygwrun"

#define CYGWRUN_LICENSE_SHORT   \
    "Licensed under the Apache-2.0 License"

#endif /* _CYGWRUN_H_INCLUDED_ */
