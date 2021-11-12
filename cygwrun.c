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

#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <errno.h>
#include <process.h>
#include <fcntl.h>
#include <io.h>

#include "cygwrun.h"

#define IS_PSW(_c)        (((_c) == L'/') || ((_c) == L'\\'))
#define IS_EMPTY_WCS(_s)  (((_s) == 0)    || (*(_s) == L'\0'))

static int      xrunexec  = 1;
static int      xdumpenv  = 0;
static wchar_t *posixroot = NULL;

static const wchar_t *pathmatches[] = {
    L"/cygdrive/?/*",
    L"/?/*",
    L"/bin/*",
    L"/clang*/*",
    L"/dev/*",
    L"/dir/*",
    L"/etc/*",
    L"/home/*",
    L"/lib*/*",
    L"/media/*",
    L"/mingw*/*",
    L"/mnt/*",
    L"/opt/*",
    L"/proc/*",
    L"/root/*",
    L"/run/*",
    L"/sbin/*",
    L"/tmp/*",
    L"/ucrt64/*",
    L"/usr/*",
    L"/var/*",
    NULL
};

static const wchar_t *pathfixed[] = {
    L"/bin",
    L"/etc",
    L"/home",
    L"/lib",
    L"/lib64",
    L"/media",
    L"/opt",
    L"/root",
    L"/run",
    L"/sbin",
    L"/tmp",
    L"/usr",
    L"/var",
    NULL
};

static const wchar_t *removeext[] = {
    L"ACLOCAL_PATH=",
    L"CONFIG_SITE=",
    L"CONICON=",
    L"CONTITLE=",
    L"INFOPATH=",
    L"MANPATH=",
    L"MINGW_CHOST=",
    L"MINGW_PACKAGE_PREFIX=",
    L"MINGW_PREFIX=",
    L"MSYSCON=",
    L"MSYSTEM=",
    L"MSYSTEM_CARCH=",
    L"MSYSTEM_CHOST=",
    L"MSYSTEM_PREFIX=",
    L"OLDPWD=",
    L"PKG_CONFIG_PATH=",
    L"SHELL=",
    L"TERM=",
    L"TERM_PROGRAM=",
    L"TERM_PROGRAM_VERSION=",
    L"XDG_DATA_DIRS=",
    NULL
};

static const wchar_t *removeenv[] = {
    L"_=",
    L"!::=",
    L"!;=",
    L"CYGWIN_ROOT=",
    L"ORIGINAL_PATH=",
    L"ORIGINAL_TEMP=",
    L"ORIGINAL_TMP=",
    L"PATH=",
    L"POSIX_ROOT=",
    L"PS1=",
    L"temp=",
    L"tmp=",
    NULL
};

static const wchar_t *posixrenv[] = {
    L"_POSIX_ROOT",
    L"CYGWIN_ROOT",
    L"POSIX_ROOT",
    NULL
};

static const wchar_t *rootpaths[] = {
    L"\\usr\\local\\bin\\",
    L"\\usr\\bin\\",
    L"\\bin\\",
    NULL
};

static int usage(int rv)
{
    FILE *os = rv == 0 ? stdout : stderr;
    fputs("\nUsage " PROJECT_NAME " [OPTIONS]... PROGRAM [ARGUMENTS]...\n", os);
    fputs("Execute PROGRAM [ARGUMENTS]...\n\nOptions are:\n", os);
    fputs(" -r <DIR>  use DIR as posix root\n", os);
    fputs(" -w <DIR>  change working directory to DIR before calling PROGRAM\n", os);
    fputs(" -k        keep extra posix environment variables.\n", os);
    fputs(" -p        print arguments instead executing PROGRAM.\n", os);
    fputs(" -e        print current environment block end exit.\n", os);
    fputs(" -v        print version information and exit.\n", os);
    fputs(" -h        print this screen and exit.\n\n", os);
    return rv;
}

static int version(void)
{
    fputs(PROJECT_NAME " version " PROJECT_VERSION_STR \
          " (" __DATE__ " " __TIME__ ")\n", stdout);
    return 0;
}

static int invalidarg(const wchar_t *arg)
{
    fwprintf(stderr, L"Unknown option: %s\n", arg);
    return usage(EINVAL);
}

static void *xmalloc(size_t size)
{
    void *p = calloc(size, 1);
    if (p == NULL) {
        _wperror(L"xmalloc");
        _exit(1);
    }
    return p;
}

static wchar_t *xwalloc(size_t size)
{
    wchar_t *p = (wchar_t *)calloc(size, sizeof(wchar_t));
    if (p == NULL) {
        _wperror(L"xwalloc");
        _exit(1);
    }
    return p;
}

static wchar_t **waalloc(size_t size)
{
    return (wchar_t **)xmalloc((size + 1) * sizeof(wchar_t *));
}

static void xfree(void *m)
{
    if (m != NULL)
        free(m);
}

static void waafree(wchar_t **array)
{
    wchar_t **ptr = array;

    if (array == NULL)
        return;
    while (*ptr != NULL)
        xfree(*(ptr++));
    free(array);
}

static wchar_t *xwcsdup(const wchar_t *s)
{
    wchar_t *p;
    size_t   n;

    if (IS_EMPTY_WCS(s))
        return NULL;
    n = wcslen(s);
    p = xwalloc(n + 2);
    wmemcpy(p, s, n);
    return p;
}

static wchar_t *xgetenv(const wchar_t *s)
{
    wchar_t *d;

    if (IS_EMPTY_WCS(s))
        return NULL;
    d = _wgetenv(s);
    if (IS_EMPTY_WCS(d))
        return NULL;
    else
        return xwcsdup(d);
}

static size_t xwcslen(const wchar_t *s)
{
    if (IS_EMPTY_WCS(s))
        return 0;
    else
        return wcslen(s);
}

static wchar_t *xwcsconcat(const wchar_t *s1, const wchar_t *s2)
{
    wchar_t *cp, *rv;
    size_t l1;
    size_t l2;

    l1 = xwcslen(s1);
    l2 = xwcslen(s2);

    if ((l1 + l2) == 0)
        return NULL;
    cp = rv = xwalloc(l1 + l2 + 2);

    if(l1 > 0)
        wmemcpy(cp, s1, l1);
    cp += l1;
    if(l2 > 0)
        wmemcpy(cp, s2, l2);
    return rv;
}

/**
 * Match = 0, NoMatch = 1, Abort = -1
 * Based loosely on sections of wildmat.c by Rich Salz
 */
static int xwcsmatch(const wchar_t *wstr, const wchar_t *wexp)
{
    int match;

    for ( ; *wexp != L'\0'; wstr++, wexp++) {
        if (*wstr == L'\0' && *wexp != L'*')
            return -1;
        switch (*wexp) {
            case L'*':
                wexp++;
                while (*wexp == L'*') {
                    /**
                     * Skip multiple stars
                     */
                    wexp++;
                }
                if (*wexp == L'\0')
                    return 0;
                while (*wstr != L'\0') {
                    int rv;
                    if ((rv = xwcsmatch(wstr++, wexp)) != 1)
                        return rv;
                }
                return -1;
            break;
            case L'?':
                if (isalpha(*wstr & 0x7F) == 0)
                    return 1;
            break;
            case L'#':
                if (isdigit(*wstr & 0x7F) == 0)
                    return 1;
            break;
            case L'[':
                match = 0;
                while (*wexp != L']') {
                    wexp++;
                    if (*wexp == L'\0')
                        return -1;
                    if (*wexp == *wstr)
                        match = 1;
                }
                if (match == 0)
                    return 1;
            break;
            default:
                if (*wstr != *wexp)
                    return 1;
            break;
        }
    }
    return (*wstr != L'\0');
}

static int strstartswith(const wchar_t *str, const wchar_t *src)
{
    while (*str != L'\0') {
        if (*str != *src)
            break;
        str++;
        src++;
        if (*src == L'\0')
            return 1;
    }
    return 0;
}

static wchar_t *strendswith(wchar_t *str, const wchar_t *src)
{
    size_t l1;
    size_t l2;

    l1 = wcslen(str);
    l2 = wcslen(src);

    if (l1 >= l2) {
        if (wcscmp(str + l1 - l2, src) == 0)
            return str + l1 - l2;
    }
    return NULL;
}

static int iswinpath(const wchar_t *s)
{
    if (s[0] < 128) {
        if (s[0] == L'\\' && s[1] == L'\\')
            return 1;
        if (isalpha(s[0]) && s[1] == L':') {
            if (IS_PSW(s[2]) || s[2] == L'\0')
                return 1;
        }
    }
    return 0;
}

static int isdotpath(const wchar_t *s)
{
    int dots = 0;

    while ((*(s++) == L'.') && (++dots < 3)) {
        if (IS_PSW(*s) || *s == L'\0')
            return 300;
    }
    return 0;
}

static int isposixpath(const wchar_t *str)
{
    int i = 0;

    if (str[0] != L'/') {
        /**
         * Check for .[/] or ..[/]
         */
        return isdotpath(str);
    }

    if (str[1] == L'\0')
        return 301;
    if (wcscmp(str, L"/dev/null") == 0)
        return 302;
    if (wcschr(str + 1, L'/') == 0) {
        /**
         * No additional slashes
         */
        const wchar_t **mp = pathfixed;
        while (mp[i] != 0) {
            if (wcscmp(str, mp[i]) == 0)
                return i + 200;
            i++;
        }
    }
    else {
        const wchar_t **mp = pathmatches;
        while (mp[i] != 0) {
            if (xwcsmatch(str, mp[i]) == 0)
                return i + 100;
            i++;
        }
    }
    return 0;
}

/**
 * Check if the argument is command line option
 * containing a posix path as value.
 * Eg. name[:value] or name[=value] will try to
 * convert value part to Windows paths unless the
 * name part itself is a path
 */
static wchar_t *cmdoptionval(wchar_t *s)
{
    while (*(++s) != L'\0') {
        if (IS_PSW(*s) || iswspace(*s))
            return NULL;
        if (*s == L'=' || *s == L':')
            return s + 1;
    }
    return NULL;
}

static int envsort(const void *arg1, const void *arg2)
{
    return _wcsicoll(*((wchar_t **)arg1), *((wchar_t **)arg2));
}

static void replacepathsep(wchar_t *s)
{
    while (*s != L'\0') {
        if (*s == L'/')
            *s = L'\\';
        s++;
    }
}

static DWORD xgetppid(DWORD pid)
{
    DWORD           p = 0;
    HANDLE          h;
    PROCESSENTRY32W e;

    h = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (h == INVALID_HANDLE_VALUE)
        return 0;

    e.dwSize = (DWORD)sizeof(PROCESSENTRY32W);
    if (Process32FirstW(h, &e)) {
        do {
            if (e.th32ProcessID == pid) {
                /**
                 * We found ourself :)
                 */
                p = e.th32ParentProcessID;
                break;
            }

        } while (Process32NextW(h, &e));
    }
    CloseHandle(h);
    return p;
}

static wchar_t *xgetpexe(DWORD pid)
{
    wchar_t  buf[8192];
    wchar_t *sp = NULL;
    DWORD    n, ppid;
    HANDLE   h;

    ppid = xgetppid(pid);
    if (ppid == 0)
        return NULL;
    h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, ppid);
    if (IS_INVALID_HANDLE(h))
        return NULL;
    if ((n = GetModuleFileNameExW(h, NULL, buf, 8192)) != 0) {
        if ((n > 5) && (n < _MAX_FNAME)) {
            /**
             * Strip leading \\?\ for short paths
             * but not \\?\UNC\* paths
             */
            if ((buf[0] == L'\\') &&
                (buf[1] == L'\\') &&
                (buf[2] == L'?')  &&
                (buf[3] == L'\\') &&
                (buf[5] == L':')) {
                wmemmove(buf, buf + 4, n - 3);
            }
        }
        sp = xwcsdup(buf);
    }
    CloseHandle(h);
    return sp;
}

static wchar_t **splitpath(const wchar_t *s, int *tokens)
{
    int c = 0;
    int x = 0;
    wchar_t **sa;
    const wchar_t *e;

    e = s;
    while (*e != L'\0') {
        if (*(e++) == L':')
            x++;
    }
    sa = waalloc(x + 1);
    while ((e = wcschr(s, L':')) != NULL) {
        wchar_t *p;
        size_t   n = (size_t)(e - s);

        if (n == 0) {
            s = e + 1;
            continue;
        }
        p = xwalloc(n + 2);
        wmemcpy(p, s, n);
        sa[c++] = p;
        s = e + 1;
        /**
         * Is the previous token a path or a flag
         */
        if (isposixpath(p)) {
            while (*s == L':') {
                /**
                 * Drop multiple trailing colons
                 */
                s++;
            }
            if (*s == L'\0')
                break;
        }
        else {
            /**
             * Not a posix path.
             * Do not split any more.
             */
            p[n] = L':';
            break;
        }
    }
    if (*s != L'\0')
        sa[c++] = xwcsdup(s);

    *tokens = c;
    return sa;
}

static wchar_t *mergepath(const wchar_t **pp)
{
    int  i, sc = 0;
    size_t len = 0;
    wchar_t  *r, *p;

    for (i = 0; pp[i] != NULL; i++) {
        len += wcslen(pp[i]) + 1;
    }
    r = p = xwalloc(len + 2);
    for (i = 0; pp[i] != NULL; i++) {
        len = wcslen(pp[i]);
        if (sc)
            *(p++) = L';';
        /**
         * Do not add semicolon before next path
         */
        if (pp[i][len - 1] == L':')
            sc = 0;
        else
            sc = 1;
        wmemcpy(p, pp[i], len);
        p += len;
    }
    return r;
}

/**
 * Remove trailing backslash and path separator(s)
 * so that we don't have problems with quoting
 * or appending
 */
static void rmtrailingsep(wchar_t *s)
{
    int i = (int)xwcslen(s);

    while (--i > 1) {
        if (IS_PSW(s[i]) || (s[i] == L';'))
            s[i] = L'\0';
        else
            break;
    }
}

static wchar_t *posixtowin(wchar_t *pp)
{
    int m;
    wchar_t *rv;
    wchar_t  windrive[] = { L'\0', L':', L'\\', L'\0'};

    if ((*pp == L'\'') || (wcschr(pp, L'/') == NULL))
        return pp;
    /**
     * Check for special paths
     */
    m = isposixpath(pp);
    if (m == 0) {
        /**
         * Not a posix path
         */
        if (iswinpath(pp))
            replacepathsep(pp);
        return pp;
    }
    else if (m == 100) {
        /**
         * /cygdrive/x/... absolute path
         */
        windrive[0] = towupper(pp[10]);
        rv = xwcsconcat(windrive, pp + 12);
        replacepathsep(rv + 3);
    }
    else if (m == 101) {
        /**
         * /x/... msys2 absolute path
         */
        windrive[0] = towupper(pp[1]);
        if (windrive[0] != *posixroot)
            return pp;
        rv = xwcsconcat(windrive, pp + 3);
        replacepathsep(rv + 3);
    }
    else if (m == 300) {
        replacepathsep(pp);
        return pp;
    }
    else if (m == 301) {
        rv = xwcsdup(posixroot);
    }
    else if (m == 302) {
        rv = xwcsdup(L"NUL");
    }
    else {
        replacepathsep(pp);
        rv = xwcsconcat(posixroot, pp);
    }
    xfree(pp);
    return rv;
}

static wchar_t *towinpath(const wchar_t *str)
{
    wchar_t *wp = NULL;

    if (iswinpath(str)) {
        wp = xwcsdup(str);
        replacepathsep(wp);
    }
    else if (xwcsmatch(str, L"*/*:/*")) {
        /**
         * Posix path separator not found.
         * No need to split/merge since we have
         * only one path element
         */
        wp = xwcsdup(str);
        wp = posixtowin(wp);
    }
    else {
        int i, n;
        wchar_t **pa = splitpath(str, &n);

        for (i = 0; i < n; i++) {
            pa[i] = posixtowin(pa[i]);
        }
        wp = mergepath(pa);
        waafree(pa);
    }
    return wp;
}

static wchar_t *getposixroot(wchar_t *r)
{
    if (r == NULL) {
        const wchar_t **e = posixrenv;
        while (*e != 0) {
            if ((r = xgetenv(*e)) != NULL)
                break;
            e++;
        }
    }
    if (r != NULL) {
        rmtrailingsep(r);
        replacepathsep(r);
    }
    else {
        wchar_t *p = xgetpexe(GetCurrentProcessId());

        if (p != NULL) {
            const wchar_t **e = rootpaths;
            wchar_t        *x = wcsrchr(p , L'\\');

            if (x != NULL) {
                x[1] = L'\0';
                while (*e != NULL) {
                    if ((x = strendswith(p, *e)) != NULL) {
                        x[0] = L'\0';
                        r = p;
                        break;
                    }
                    e++;
                }
            }
        }
        if (r == NULL) {
            /**
             * Use default location
             */
            xfree(p);
            r = xwcsconcat(xgetenv(L"SYSTEMDRIVE"), L"\\cygwin64");
        }
        p = xwcsconcat(r, L"\\etc\\fstab");
        if (_waccess(p, 0)) {
            xfree(r);
            r = NULL;
        }
        xfree(p);
    }
    return r;
}

static wchar_t *xsearchexe(const wchar_t *path, const wchar_t *name, const wchar_t **fname)
{
    HANDLE    fh = NULL;
    wchar_t  *sp = NULL;
    wchar_t  *fp;
    DWORD     ln = 0;
    DWORD     sz = _MAX_PATH;

    while (sp == NULL) {
        sp = xwalloc(sz);
        ln = SearchPathW(path, name, L".exe", sz, sp, NULL);
        if (ln == 0) {
            goto failed;
        }
        else if (ln > sz) {
            xfree(sp);
            sp = NULL;
            sz = ln;
        }
    }
    fh = CreateFileW(sp, GENERIC_READ, FILE_SHARE_READ, 0,
                     OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (IS_INVALID_HANDLE(fh))
        goto failed;
    do {
        if (ln > sz) {
            xfree(sp);
            sz = ln;
            sp = xwalloc(sz);
        }
        ln = GetFinalPathNameByHandleW(fh, sp, sz, VOLUME_NAME_DOS);
        if (ln == 0) {
            CloseHandle(fh);
            goto failed;
        }
    } while (ln > sz);

    CloseHandle(fh);
    if ((ln > 5) && (ln < _MAX_FNAME)) {
        /**
         * Strip leading \\?\ for short paths
         * but not \\?\UNC\* paths
         */
        if ((sp[0] == L'\\') &&
            (sp[1] == L'\\') &&
            (sp[2] == L'?')  &&
            (sp[3] == L'\\') &&
            (sp[5] == L':')) {
            wmemmove(sp, sp + 4, ln - 3);
            ln -= 4;
        }
    }
    if (fname != NULL) {
        fp = sp + ln;
        while (--fp > sp) {
            if (*fp == L'\\') {
                *(fp++) = L'\0';
                *fname = fp;
                break;
            }
        }
    }
    return sp;
failed:
    xfree(sp);
    return NULL;
}


static int posixmain(int argc, wchar_t **wargv, int envc, wchar_t **wenvp)
{
    int i, rc = 0;
    intptr_t rp;

    for (i = xrunexec; i < argc && xdumpenv == 0; i++) {
        wchar_t *a = wargv[i];
        size_t   n = wcslen(a);

        if ((a[0] == L'/') && (a[1] == L'/')) {
            if (wcschr(a + 2, L'/')) {
                /**
                 * Handle mingw double slash
                 */
                wmemmove(a, a + 1, n--);
            }
        }
        if ((a[0] == L'/') && (a[1] == L'\0')) {
            /**
             * Special case for / (root)
             */
            xfree(a);
            wargv[i] = xwcsdup(posixroot);
        }
        else if ((n < 4) || (wcschr(a, L'/') == 0)) {
            /**
             * Argument is too short or has no forward slashes
             */
        }
        else if (iswinpath(a)) {
            /**
             * We have something like
             * \\ or C:[/...]
             * Replace to backward slashes inplace
             */
             replacepathsep(a);
        }
        else if (isposixpath(a)) {
            /**
             * We have posix path
             */
             wargv[i] = posixtowin(a);
        }
        else {
            wchar_t *p;
            wchar_t *v = cmdoptionval(a);

            if (v != NULL) {
                p = xwcsdup(v);
                p = posixtowin(p);
               *v = L'\0';
                wargv[i] = xwcsconcat(a, p);
                xfree(p);
                xfree(a);
            }
        }
        if (xrunexec == 0) {
            if (i > 0)
                fputc(L'\n', stdout);
            fputws(wargv[i], stdout);
        }
    }
    if (xrunexec == 0 && xdumpenv == 0) {
        return 0;
    }
    for (i = 0; i < (envc - 1); i++) {
        wchar_t *v;
        wchar_t *e = wenvp[i];

        v = wcschr(e, L'=');
        if (v == NULL) {
            /**
             * Bad environment
             */
            return EBADF;
        }
        if (wcschr(++v, L'/') != NULL) {
            if ((v[0] == L'/') && (v[1] == L'\0')) {
                /**
                 * Special case for / (root)
                 */
                *v = L'\0';
                wenvp[i] = xwcsconcat(e, posixroot);
                xfree(e);
            }
            else if (wcslen(v) > 3) {
                wchar_t *p = towinpath(v);

                if (p != NULL) {
                    *v = L'\0';
                    wenvp[i] = xwcsconcat(e, p);
                    xfree(e);
                    xfree(p);
                }
            }
            else if (isdotpath(v)) {
                /**
                 * We have something like
                 * VARIABLE=./ or VARIABLE=../
                 * Replace value to backward slashes inplace
                 */
                 replacepathsep(v);
            }
        }
    }

    qsort((void *)wenvp, envc, sizeof(wchar_t *), envsort);
    if (xdumpenv) {
        int n, x;
        for (i = 0, x = 0; i < envc; i++) {
            const wchar_t *v = wenvp[i];
            if (argc > 0) {
                v = NULL;
                for (n = 0; n < argc; n++) {
                    if (strstartswith(wenvp[i], wargv[n])) {
                        v = wenvp[i];
                        break;
                    }
                }
            }
            if (v != NULL) {
                if (x++ > 0)
                    fputc(L'\n', stdout);
                fputws(v, stdout);
            }
        }
        return x ? 0 : ENOENT;
    }
    _flushall();
    rp = _wspawnvpe(_P_WAIT, wargv[0], wargv, wenvp);
    if (rp == (intptr_t)-1) {
        rc = errno;
        fwprintf(stderr, L"Cannot execute program: %s\nFatal error: %s\n",
                 wargv[0], _wcserror(rc));
        return rc;
    }
    else {
        /**
         * return the child exit code
         */
        rc = (int)rp;
    }
    return rc;
}

int wmain(int argc, const wchar_t **wargv, const wchar_t **wenv)
{
    int i;
    wchar_t **dupwargv = NULL;
    wchar_t **dupwenvp = NULL;
    wchar_t *crp       = NULL;
    wchar_t *cwd       = NULL;
    wchar_t *cpp;

    wchar_t  nnp[4]    = { L'\0', L'\0', L'\0', L'\0' };
    int dupenvc = 0;
    int dupargc = 0;
    int cextenv = 1;
    int envc    = 0;
    int opts    = 1;

    if (argc < 2)
        return usage(1);
    if (wenv == NULL)
        return invalidarg(L"missing environment");
    dupwargv = waalloc(argc);
    for (i = 1; i < argc; i++) {
        const wchar_t *p = wargv[i];

        if (opts) {
            /**
             * Simple argument parsing
             */
            if (cwd == nnp) {
                cwd = xwcsdup(p);
                continue;
            }
            if (crp == nnp) {
                crp = xwcsdup(p);
                continue;
            }

            if (p[0] == L'-') {
                if ((p[1] == L'\0') || (p[2] != L'\0'))
                    return invalidarg(p);
                switch (p[1]) {
                    case L'e':
                        xdumpenv = 1;
                        xrunexec = 0;
                        opts     = 0;
                    break;
                    case L'h':
                        return usage(0);
                    break;
                    case L'k':
                        cextenv = 0;
                    break;
                    case L'p':
                        xrunexec = 0;
                        xdumpenv = 0;
                        opts     = 0;
                    break;
                    case L'r':
                        crp = nnp;
                    break;
                    case L'v':
                        return version();
                    break;
                    case L'w':
                        cwd = nnp;
                    break;
                    default:
                        return invalidarg(p);
                    break;
                }
                continue;
            }
            opts = 0;
        }
        dupwargv[dupargc++] = xwcsdup(p);
    }
    if ((dupargc < 1) && (xdumpenv == 0)) {
        fputs("Missing PROGRAM argument\n", stderr);
        return usage(1);
    }
    if ((cwd == nnp) || (crp == nnp)) {
        fputs("Missing required parameter value\n", stderr);
        return usage(1);
    }
    if ((cpp = xgetenv(L"PATH")) == NULL) {
        fputs("Missing PATH environment variable\n", stderr);
        return ENOENT;
    }
    rmtrailingsep(cpp);
    if ((posixroot = getposixroot(crp)) == NULL) {
        fputs("Cannot find valid POSIX_ROOT\n", stderr);
        return ENOENT;
    }
    if (cwd != NULL) {
        rmtrailingsep(cwd);
        cwd = posixtowin(cwd);
        if (_wchdir(cwd) != 0) {
            fwprintf(stderr, L"Invalid working directory: '%s'\n", cwd);
            return ENOENT;
        }
        xfree(cwd);
    }
    if ((cwd = _wgetcwd(NULL, 0)) == NULL) {
        fputs("Cannot get current working directory\n", stderr);
        return ENOENT;
    }
    while (wenv[envc] != NULL) {
        ++envc;
    }

    dupwenvp = waalloc(envc + 2);
    for (i = 0; i < envc; i++) {
        const wchar_t **e;
        const wchar_t  *p = wenv[i];

        if (cextenv) {
            e = removeext;
            while (*e != NULL) {
                if (strstartswith(p, *e)) {
                    /**
                     * Skip private environment variable
                     */
                    p = NULL;
                    break;
                }
                e++;
            }
        }
        if (p != NULL) {
            e = removeenv;
            while (*e != NULL) {
                if (strstartswith(p, *e)) {
                    p = NULL;
                    break;
                }
                e++;
            }
        }
        if (p != NULL)
            dupwenvp[dupenvc++] = xwcsdup(p);
    }
    if (xrunexec) {
        wchar_t *exe;

        exe = posixtowin(dupwargv[0]);
        if (_waccess(exe, 0)) {
            wchar_t *sch;
            wchar_t *pp1;
            wchar_t *pp2;
            /**
             * PROGRAM is not an absolute path.
             * Add current directory as first PATH element
             * and search for PROGRAM[.exe]
             */
            pp1 = xwcsconcat(cwd, L";");
            pp2 = xwcsconcat(pp1, cpp);

            sch = xsearchexe(pp2, exe, NULL);
            if (sch == NULL) {
                fwprintf(stderr, L"Cannot find PROGRAM '%s'\n", exe);
                return ENOENT;
            }
            xfree(pp1);
            xfree(pp2);
            xfree(exe);
            exe = sch;
        }
        dupwargv[0] = exe;
        dupwenvp[dupenvc++] = xwcsconcat(L"_POSIX_ROOT=", posixroot);
    }
    /**
     * Add back environment variables
     */
    dupwenvp[dupenvc++] = xwcsconcat(L"PATH=", cpp);
    xfree(cpp);
    xfree(cwd);
    return posixmain(dupargc, dupwargv, dupenvc, dupwenvp);
}
