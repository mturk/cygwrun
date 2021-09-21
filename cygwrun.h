/*
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#ifndef _CYGWRUN_H_INCLUDED_
#define _CYGWRUN_H_INCLUDED_

/**
 * Disable or reduce the frequency of...
 *   C4100: unreferenced formal parameter
 *   C4702: unreachable code
 *   C4244: int to char/short - precision loss
 */
#if defined(_MSC_VER)
# pragma warning(disable: 4100 4244 4702)
#endif

#define PROJECT_NAME            "cygwrun"
#define PROJECT_PNAME           "cygwrun"
#define PROJECT_VERSION_STR     "1.0.2"
#define PROJECT_VERSION_CSV      1,0,2

#define PROJECT_COPYRIGHT       "Copyright(c) 1964-2021 Mladen Turk"
#define PROJECT_COMPANY_NAME    "Acme Corporation"
#define PROJECT_DESCRIPTION     "Run windows applications under posix environment"


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
