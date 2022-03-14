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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <errno.h>
#include <process.h>
#include <fcntl.h>
#include <io.h>

#include "cygwrun.h"

#define IS_PSW(_c)        (((_c) == L'/') || ((_c)  == L'\\'))
#define IS_EMPTY_WCS(_s)  (((_s) == NULL) || (*(_s) == L'\0'))
#define IS_VALID_WCS(_s)  (((_s) != NULL) && (*(_s) != L'\0'))

static int      xrunexec  = 1;
static int      xdumpenv  = 0;
static int      xskipenv  = 0;
static int      xshowerr  = 1;
static wchar_t *posixroot = NULL;

static const wchar_t *pathmatches[] = {
    L"/cygdrive/?/+*",
    L"/bin/*",
    L"/dev/*",
    L"/dir/*",
    L"/etc/*",
    L"/home/*",
    L"/lib/*",
    L"/lib64/*",
    L"/media/*",
    L"/mnt/*",
    L"/opt/*",
    L"/proc/*",
    L"/root/*",
    L"/run/*",
    L"/sbin/*",
    L"/tmp/*",
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
    L"INFOPATH",
    L"MANPATH",
    L"PROFILEREAD",
    L"SHELL",
    L"SHLVL",
    L"TERM",
    NULL
};

static const wchar_t *removeenv[] = {
    L"_",
    L"!::",
    L"!;",
    L"CYGWIN_ROOT",
    L"OLDPWD",
    L"ORIGINAL_PATH",
    L"ORIGINAL_TEMP",
    L"ORIGINAL_TMP",
    L"PATH",
    L"POSIX_ROOT",
    L"PS1",
    L"temp",
    L"tmp",
    NULL
};

static const wchar_t *posixrenv[] = {
    L"CYGWIN_ROOT",
    L"POSIX_ROOT",
    NULL
};

const char xisvarchar[128] =
{
    /** Reject all ctrl codes...                                          */
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    /**   ! " # $ % & ' ( ) * + , - . /  0 1 2 3 4 5 6 7 8 9 : ; < = > ?  */
        0,0,0,0,3,3,3,0,3,3,0,3,3,1,1,0, 1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,3,
    /** @ A B C D E F G H I J K L M N O  P Q R S T U V W X Y Z [ \ ] ^ _  */
        3,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,3,0,3,0,1,
    /** ` a b c d e f g h i j k l m n o  p q r s t u v w x y z { | } ~    */
        0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,3,0,3,3,0
};

static int usage(int rv)
{
    if (xshowerr) {
        FILE *os = rv == 0 ? stdout : stderr;
        fputs("\nUsage " PROJECT_NAME " [OPTIONS]... PROGRAM [ARGUMENTS]...\n", os);
        fputs("Execute PROGRAM [ARGUMENTS]...\n\nOptions are:\n", os);
        fputs(" -r <DIR>  use DIR as posix root\n", os);
        fputs(" -w <DIR>  change working directory to DIR before calling PROGRAM\n", os);
        fputs(" -k        keep extra posix environment variables.\n", os);
        fputs(" -s        do not translate environment variables.\n", os);
        fputs(" -q        do not print errors to stderr.\n", os);
        fputs(" -v        print version information and exit.\n", os);
        fputs(" -h        print this screen and exit.\n", os);
        fputs(" -p        print arguments instead executing PROGRAM.\n", os);
        fputs(" -e        print current environment block end exit.\n", os);
        fputs("           if defined, only print variables that begin with ARGUMENTS.\n\n", os);
    }
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
    if (xshowerr)
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
    int mflag;

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
                if ((*wstr < L'A') || (*wstr > L'z') || (isalpha(*wstr) == 0))
                    return -1;
            break;
            case L'+':
                /**
                 * Any character
                 */
            break;
            case L'[':
                match = 0;
                mflag = 0;
                if (*(++wexp) == L'!') {
                    /**
                     * Negate range
                     */
                    wexp++;
                    if (*wexp != L']')
                        mflag = 1;
                }
                while (*wexp != L']') {
                    if ((*wexp == L'\0') || (*wexp == L'['))
                        return -1;
                    if (*wexp == *wstr)
                        match = 1;
                    wexp++;
                }
                if (match == mflag)
                    return -1;
            break;
            default:
                if (*wstr != *wexp)
                    return 1;
            break;
        }
    }
    return (*wstr != L'\0');
}

static int xwcsisenvvar(const wchar_t *str, const wchar_t *var)
{
    while (*str != L'\0') {
        if (*str != *var)
            break;
        str++;
        var++;
        if (*var == L'\0')
            return *str == L'=';
    }
    return 0;
}

static int iswinpath(const wchar_t *s)
{
    if (s[0] < 128) {
        if (s[0] == L'\\')
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
 * Eg. [/]name[:value] or [--]name[=value] will try to
 * convert value part to Windows paths unless the
 * name part itself is a path
 */
static wchar_t *cmdoptionval(wchar_t *v)
{
    int n      = 0;
    wchar_t *s = v;

    if (*s == L'/') {
        s++;
    }
    else if (*s == L'-') {
        if (*(++s) == L'-')
            s++;
    }
    while (*s != L'\0') {
        int c = *(s++);
        if (c >= 127)
            return NULL;
        if (c == L':') {
            if (*v != L'/' || n == 0)
                return NULL;
            else
                return s;
        }
        else if (c == L'=') {
            return n ? s : NULL;
        }
        if (xisvarchar[c] == 0)
            return NULL;
        n++;
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
            /**
             * Handle multiple collons
             */
            if (c == 0) {
                waafree(sa);
                return NULL;
            }
            else {
                s = e + 1;
                continue;
            }
        }
        p = xwalloc(n + 2);
        wmemcpy(p, s, n);
        sa[c++] = p;
        if (isposixpath(p) == 0) {
            /**
             * Token before ':' was not a posix path.
             * Return original str regerdless if some
             * previous tokens evaluated as posix path.
             */
            waafree(sa);
            return NULL;
        }
        s = e + 1;
    }
    if (*s != L'\0') {
        if (isposixpath(s)) {
            sa[c++] = xwcsdup(s);
        }
        else {
            waafree(sa);
            return NULL;
        }
    }

    *tokens = c;
    return sa;
}

static wchar_t *mergepath(const wchar_t **pp)
{
    int  i;
    size_t len = 0;
    wchar_t  *r, *p;

    for (i = 0; pp[i] != NULL; i++) {
        len += wcslen(pp[i]) + 1;
    }
    r = p = xwalloc(len + 2);
    for (i = 0; pp[i] != NULL; i++) {
        len = wcslen(pp[i]);
        if (i > 0)
            *(p++) = L';';
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
static void rmtrailingpsep(wchar_t *s)
{
    int i = (int)xwcslen(s) - 1;

    while (i > 2) {
        if (s[i] == L';')
            s[i--] = L'\0';
        else
            break;
    }
    while (i > 1) {
        if (IS_PSW(s[i]))
            s[i--] = L'\0';
        else
            break;
    }
}

static wchar_t *posixtowin(wchar_t *pp)
{
    int m;
    wchar_t *rv;
    wchar_t  windrive[] = { L'\0', L':', L'\\', L'\0'};

    if (wcschr(pp, L'/') == NULL) {
        /**
         * Nothing to do.
         */
        return pp;
    }
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
    wchar_t *wp;

    if (iswinpath(str)) {
        wp = xwcsdup(str);
        replacepathsep(wp);
    }
    else if (xwcsmatch(str, L"*/+*:*/+*") == 0) {
        /**
         * We have [...]/x[...]:[...]/x[...]
         * which can be eventually converted
         * to token1;token2[;tokenN...]
         * If any of tokens is not a posix path
         * return original string.
         */
        int i, n;
        wchar_t **pa = splitpath(str, &n);

        if (pa != NULL) {
            for (i = 0; i < n; i++) {
                pa[i] = posixtowin(pa[i]);
            }
            wp = mergepath(pa);
            waafree(pa);
        }
        else {
            wp = xwcsdup(str);
        }
    }
    else {
        wp = xwcsdup(str);
        wp = posixtowin(wp);
    }
    return wp;
}

static wchar_t *getposixroot(wchar_t *r)
{

    if (r == NULL) {
        const wchar_t **e = posixrenv;

        if ((r = xgetenv(L"_CYGWRUN_POSIX_ROOT")) != NULL)
            return r;
        while (*e != NULL) {
            if ((r = xgetenv(*e)) != NULL)
                break;
            e++;
        }
        if (r == NULL) {
            /**
             * Use default locations
             */
            r = xwcsdup(L"C:\\cygwin64\\etc\\fstab");
            if (_waccess(r, 0)) {
                wcscpy(r, L"C:\\cygwin\\etc\\fstab");
                if (_waccess(r, 0)) {
                    xfree(r);
                    return NULL;
                }
                r[9]  = L'\0';
            }
            else {
                r[11] = L'\0';
            }
            return r;
        }
    }
    if (r != NULL) {
        rmtrailingpsep(r);
        replacepathsep(r);
       *r = towupper(*r);
    }
    return r;
}

static wchar_t *xsearchexe(const wchar_t *paths, const wchar_t *name)
{
    HANDLE    fh = NULL;
    wchar_t  *sp = NULL;
    DWORD     ln = 0;
    DWORD     sz = _MAX_PATH;

    while (sp == NULL) {
        sp = xwalloc(sz);
        ln = SearchPathW(paths, name, L".exe", sz, sp, NULL);
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
    return sp;
failed:
    xfree(sp);
    return NULL;
}


static int posixmain(int argc, wchar_t **wargv, int envc, wchar_t **wenvp)
{
    int i, rc = 0;
    intptr_t rp;

    if (xdumpenv == 0) {
        for (i = xrunexec; i < argc; i++) {
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
            else if (a[0] == L'\'') {
                /**
                 * Do not convert arguments enclosed
                 * in single qoutes.
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
            else if ((n < 4) || (wcschr(a, L'/') == 0)) {
                /**
                 * Argument is too short or has no forward slashes
                 */
            }
            else if (isposixpath(a)) {
                /**
                 * We have posix path
                 */
                 wargv[i] = posixtowin(a);
            }
            else {
                wchar_t *v = cmdoptionval(a);

                if (IS_VALID_WCS(v) && (*v != L'\'')) {
                    if (iswinpath(v)) {
                        replacepathsep(v);
                    }
                    else if (isposixpath(v)) {
                        wchar_t *p = posixtowin(xwcsdup(v));

                        v[0] = L'\0';
                        wargv[i] = xwcsconcat(a, p);
                        xfree(p);
                        xfree(a);
                    }
                }
            }
            if (xrunexec == 0) {
                if (i > 0)
                    fputwc(L'\n', stdout);
                fputws(wargv[i], stdout);
            }
        }
        if (xrunexec == 0) {
            return 0;
        }
    }
    if (xskipenv == 0) {
        for (i = 0; i < (envc - xrunexec - 1); i++) {
            wchar_t *v;
            wchar_t *e = wenvp[i];

            v = wcschr(e, L'=');
            if (v == NULL) {
                /**
                 * Bad environment
                 */
                return EBADF;
            }
            v++;
            if ((*v != L'\'') && (wcschr(v, L'/') != NULL)) {
                if ((v[0] == L'/') && (v[1] == L'\0')) {
                    /**
                     * Special case for / (root)
                     */
                    v[0] = L'\0';
                    wenvp[i] = xwcsconcat(e, posixroot);
                    xfree(e);
                }
                else if (wcslen(v) > 3) {
                    wchar_t *p = towinpath(v);

                    v[0] = L'\0';
                    wenvp[i] = xwcsconcat(e, p);
                    xfree(e);
                    xfree(p);
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
    }
    qsort((void *)wenvp, envc, sizeof(wchar_t *), envsort);
    if (xdumpenv) {
        int n, x;
        for (i = 0, x = 0; i < envc; i++) {
            const wchar_t *v = wenvp[i];
            if (argc > 0) {
                v = NULL;
                for (n = 0; n < argc; n++) {
                    if (xwcsisenvvar(wenvp[i], wargv[n])) {
                        v = wenvp[i];
                        break;
                    }
                }
            }
            if (v != NULL) {
                if (x++ > 0)
                    fputwc(L'\n', stdout);
                fputws(v, stdout);
            }
        }
        return x ? 0 : ENOENT;
    }
    _flushall();
    rp = _wspawnvpe(_P_WAIT, wargv[0], wargv, wenvp);
    if (rp == (intptr_t)-1) {
        rc = errno;
        if (xshowerr)
            fwprintf(stderr, L"Failed to execute: '%s'\n", wargv[0]);
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
    int clreenv = 1;
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
                    case L'k':
                        clreenv  = 0;
                    break;
                    case L'e':
                        xdumpenv = 1;
                        xrunexec = 0;
                        opts     = 0;
                    break;
                    case L'p':
                        xrunexec = 0;
                        xdumpenv = 0;
                        opts     = 0;
                    break;
                    case L'r':
                        crp = nnp;
                    break;
                    case L's':
                        xskipenv = 1;
                    break;
                    case L'q':
                        xshowerr = 0;
                    break;
                    case L'w':
                        cwd = nnp;
                    break;
                    case L'v':
                    case L'V':
                        return version();
                    break;
                    case L'h':
                    case L'H':
                    case L'?':
                        xshowerr = 1;
                        return usage(0);
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
    if ((dupargc == 0) && (xdumpenv == 0)) {
        if (xshowerr)
            fputs("Missing PROGRAM argument\n", stderr);
        return usage(1);
    }
    if ((cwd == nnp) || (crp == nnp)) {
        if (xshowerr)
            fputs("Missing required parameter value\n", stderr);
        return usage(1);
    }
    if ((cpp = xgetenv(L"PATH")) == NULL) {
        if (xshowerr)
            fputs("Missing PATH environment variable\n", stderr);
        return ENOENT;
    }
    rmtrailingpsep(cpp);
    if ((posixroot = getposixroot(crp)) == NULL) {
        if (xshowerr)
            fputs("Cannot find valid POSIX_ROOT\n", stderr);
        return ENOENT;
    }
    if (cwd != NULL) {
        rmtrailingpsep(cwd);
        cwd = posixtowin(cwd);
        if (_wchdir(cwd) != 0) {
            if (xshowerr)
                fwprintf(stderr, L"Invalid working directory: '%s'\n", cwd);
            return ENOENT;
        }
        xfree(cwd);
    }
    if ((cwd = _wgetcwd(NULL, 0)) == NULL) {
        if (xshowerr)
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

        if (clreenv) {
            e = removeext;
            while (*e != NULL) {
                if (xwcsisenvvar(p, *e)) {
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
                if (xwcsisenvvar(p, *e)) {
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

            sch = xsearchexe(pp2, exe);
            if (sch == NULL) {
                if (xshowerr)
                    fwprintf(stderr, L"Cannot find PROGRAM '%s'\n", exe);
                return ENOENT;
            }
            xfree(pp1);
            xfree(pp2);
            xfree(exe);
            exe = sch;
        }
        dupwargv[0] = exe;
        dupwenvp[dupenvc++] = xwcsconcat(L"_CYGWRUN_POSIX_ROOT=", posixroot);
    }
    /**
     * Add back environment variables
     */
    dupwenvp[dupenvc++] = xwcsconcat(L"PATH=", cpp);
    xfree(cpp);
    xfree(cwd);
    return posixmain(dupargc, dupwargv, dupenvc, dupwenvp);
}
