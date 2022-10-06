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

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static int xwcsisienvvar(const wchar_t *str, const wchar_t *var)
{
    while (*str != L'\0') {
        if (towlower(*str) != towlower(*var))
            break;
        str++;
        var++;
        if (*var == L'\0')
            return *str == L'=';
    }
    return 0;
}

int wmain(int argc, const wchar_t **wargv, const wchar_t **wenv)
{
    int x = 0;
    int e = 0;

    if (argc == 1) {
        fwprintf(stdout, L"_=%s", wargv[0]);
        x++;
    }
    while (wenv[e] != NULL) {
        const wchar_t *v = wenv[e];
        if (argc > 1) {
            int i;
            v = NULL;
            for (i = 1; i < argc; i++) {
                if (xwcsisienvvar(wenv[e], wargv[i])) {
                    v = wenv[e];
                    break;
                }
            }
        }
        if (v != NULL) {
            if (x++ > 0)
                fputwc(L'\n', stdout);
            fputws(v, stdout);
        }
        e++;
    }

    return x ? 0 : ENOENT;
}
