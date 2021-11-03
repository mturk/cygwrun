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

#define IS_INVALID_HANDLE(h) (((h) == 0 || (h) == INVALID_HANDLE_VALUE))
#define SAFE_CLOSE_HANDLE(_h)                                       \
    if (((_h) != NULL) && ((_h) != INVALID_HANDLE_VALUE))           \
        CloseHandle((_h));                                          \
    (_h) = NULL
/** Align to power of 2 boundary */
#define ALIGN_TO_BOUNDARY(_s, _b) \
    (((_s) + ((_b) - 1)) & ~((_b) - 1))
/** Default alignment */
#define DEFAULT_ALIGNMENT       8
#define ALIGN_DEFAULT(_s)       ALIGN_TO_BOUNDARY(_s, DEFAULT_ALIGNMENT)

/**
 * Version info
 */
#define PROJECT_MAJOR_VERSION   1
#define PROJECT_MINOR_VERSION   0
#define PROJECT_PATCH_VERSION   6
#if defined(_VENDOR_NUM)
# define PROJECT_MICRO_VERSION  _VENDOR_NUM
#else
# define PROJECT_MICRO_VERSION  0
#endif

/**
 * Helper macros for properly quoting a value as a
 * string in the C preprocessor
 */
#define CPP_TOSTR_HELPER(n)     #n
#define CPP_TOSTR(n)            CPP_TOSTR_HELPER(n)

#if defined(_VENDOR_SFX)
# define PROJECT_VENDOR_SFX     CPP_TOSTR(_VENDOR_SFX)
#else
# define PROJECT_VENDOR_SFX     ""
#endif
/**
 * Set to zero for non dev versions
 */
#define PROJECT_ISDEV_VERSION   0

#if PROJECT_ISDEV_VERSION
# define PROJECT_VERSION_SFX    PROJECT_VENDOR_SFX "-dev"
#else
# define PROJECT_VERSION_SFX    PROJECT_VENDOR_SFX
#endif

/**
 * Macro for .rc files using numeric csv representation
 */
#define PROJECT_VERSION_CSV     PROJECT_MAJOR_VERSION,                          \
                                PROJECT_MINOR_VERSION,                          \
                                PROJECT_PATCH_VERSION,                          \
                                PROJECT_MICRO_VERSION

#define PROJECT_VERSION_STR                                                     \
                                CPP_TOSTR(PROJECT_MAJOR_VERSION) "."            \
                                CPP_TOSTR(PROJECT_MINOR_VERSION) "."            \
                                CPP_TOSTR(PROJECT_PATCH_VERSION)                \
                                PROJECT_VERSION_SFX

#define PROJECT_NAME            "cygwrun"
#define PROJECT_COPYRIGHT       "Copyright(c) 1964-2021 Mladen Turk"
#define PROJECT_COMPANY_NAME    "Acme Corporation"
#define PROJECT_DESCRIPTION     "Run windows applications under posix environment"

#define PROJECT_LICENSE_SHORT \
    "Licensed under the Apache-2.0 License"
#define PROJECT_LICENSE \
  "Licensed under the Apache License, Version 2.0 (the ""License""); "          \
  "you may not use this file except in compliance with the License. "           \
  "You may obtain a copy of the License at\r\n\r\n"                             \
  "http://www.apache.org/licenses/LICENSE-2.0\r\n\r\n"                          \
  "Unless required by applicable law or agreed to in writing, software "        \
  "distributed under the License is distributed on an ""AS IS"" BASIS, "        \
  "WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  "  \
  "See the License for the specific language governing permissions and "        \
  "limitations under the License."


#endif /* _CYGWRUN_H_INCLUDED_ */
