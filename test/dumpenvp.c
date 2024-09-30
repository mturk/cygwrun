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

static int xisienvvar(const char *str, const char *var)
{
    while (*str) {
        if (tolower(*str) != tolower(*var))
            break;
        str++;
        var++;
        if (*var == '\0')
            return *str == '=';
    }
    return 0;
}

int main(int argc, const char **argv, const char **envp)
{
    int x = 0;
    int e = 0;

    while (envp[e] != NULL) {
        const char *v = envp[e];
        if (argc > 1) {
            int i;
            v = NULL;
            for (i = 1; i < argc; i++) {
                if (xisienvvar(envp[e], argv[i])) {
                    v = envp[e];
                    break;
                }
            }
        }
        if (v != NULL) {
            if (x++ > 0)
                fputc('\n', stdout);
            fputs(v, stdout);
        }
        e++;
    }

    return x ? 0 : ENOENT;
}
