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
static int      xforcewp  = 0;

static wchar_t *posixroot = NULL;

static const wchar_t *pathmatches[] = {
    L"/cygdrive/?/+*",
    L"/bin/*",
    L"/dev/*",
    L"/etc/*",
    L"/home/*",
    L"/lib/*",
    L"/lib64/*",
    L"/opt/*",
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
    L"/dev",
    L"/etc",
    L"/home",
    L"/lib",
    L"/lib64",
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
    L"OLDPWD",
    L"ORIGINAL_PATH",
    L"ORIGINAL_TEMP",
    L"ORIGINAL_TMP",
    NULL
};

static const wchar_t *removeenv[] = {
    L"_",
    L"!::",
    L"!;",
    L"CYGWIN_ROOT",
    L"PATH",
    L"POSIX_ROOT",
    L"PS1",
    L"PWD",
    L"temp",
    L"tmp",
    NULL
};

static const wchar_t *posixrenv[] = {
    L"CYGWIN_ROOT",
    L"POSIX_ROOT",
    NULL
};

const char xvalidvarname[128] =
{
    /** Reject all ctrl codes...                                          */
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    /**   ! " # $ % & ' ( ) * + , - . /  0 1 2 3 4 5 6 7 8 9 : ; < = > ?  */
        0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0, 1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,
    /** @ A B C D E F G H I J K L M N O  P Q R S T U V W X Y Z [ \ ] ^ _  */
        0,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2,2,2,2,0,0,0,0,1,
    /** ` a b c d e f g h i j k l m n o  p q r s t u v w x y z { | } ~    */
        0,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2,2,2,2,0,0,0,0,0
};

static wchar_t zerostring[4] = { L'\0', L'\0', L'\0', L'\0' };

static int usage(int rv)
{
    if (xshowerr) {
        FILE *os = rv == 0 ? stdout : stderr;
        fputs("\nUsage " PROJECT_NAME " [OPTIONS]... PROGRAM [ARGUMENTS]...\n", os);
        fputs("Execute PROGRAM [ARGUMENTS]...\n\nOptions are:\n", os);
        fputs(" -r <DIR>  use DIR as posix root\n", os);
        fputs(" -w <DIR>  change working directory to DIR before calling PROGRAM\n", os);
        fputs(" -k        keep extra posix environment variables.\n", os);
        fputs(" -f        convert all unknown posix absolute paths\n", os);
        fputs(" -s        do not translate environment variables.\n", os);
        fputs(" -q        do not print errors to stderr.\n", os);
        fputs(" -v        print version information and exit.\n", os);
        fputs(" -V        print detailed version information and exit.\n", os);
        fputs(" -h | -?   print this screen and exit.\n", os);
        fputs(" -p        print arguments instead executing PROGRAM.\n", os);
        fputs(" -e        print current environment block end exit.\n", os);
        fputs("           if defined, only print variables that begin with ARGUMENTS.\n\n", os);
        fputs("To file bugs, visit " PROJECT_URL, os);
    }
    return rv;
}

static int version(int verbose)
{
    if (verbose) {
        fputs(PROJECT_NAME " version " PROJECT_VERSION_STR \
              " (" __DATE__ " " __TIME__ ")\n", stdout);
        fputs("\n" PROJECT_LICENSE "\n\n", stdout);
        fputs("Visit " PROJECT_URL " for more details", stdout);
    }
    else {
        fputs(PROJECT_NAME " version " PROJECT_VERSION_STR, stdout);
    }
    return 0;
}

static int invalidarg(const wchar_t *arg)
{
    if (xshowerr)
        fprintf(stderr, "Error: unknown command line option: '%S'\n", arg);
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

static wchar_t *xwcscpaths(const wchar_t *s1, const wchar_t *s2)
{
    wchar_t *cp, *rv;
    size_t l1, l2;

    l1 = xwcslen(s1);
    l2 = xwcslen(s2);

    if (l1 == 0)
        return NULL;
    cp = rv = xwalloc(l1 + l2 + 4);

    wmemcpy(cp, s1, l1);
    cp += l1;
    if(l2 > 0) {
        *(cp++) = L';';
        wmemcpy(cp, s2, l2);
    }
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
                    int rv = xwcsmatch(wstr++, wexp);
                    if (rv != 1)
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

static int xisvarchar(int c)
{
    if ((c < 32) || (c > 127))
        return 0;
    else
        return xvalidvarname[c];

}

static int iswinpath(const wchar_t *s)
{
    int i = 0;

    if (s[0] == L'\\') {
        if ((s[1] == L'\\') &&
            (s[2] == L'?' ) &&
            (s[3] == L'\\') &&
            (s[4] != L'\0')) {
            /**
             * We have \\?\* path
             */
            i  = 4;
            s += 4;
        }
    }
    if (xisvarchar(s[0]) == 2) {
        if (s[1] == L':') {
            if (s[2] == L'\0')
                i += 2;
            else if (IS_PSW(s[2]))
                i += 3;
        }
    }
    return i;
}

static int isdotpath(const wchar_t *s)
{

    if (s[0] == L'.') {
        if (IS_PSW(s[1]))
            return 300;
        if ((s[1] == L'.') && IS_PSW(s[2]))
            return 300;
    }

    return 0;
}

static int isposixpath(const wchar_t *str)
{
    int i = 0;

    if (str[0] != L'/')
        return isdotpath(str);
    if (str[1] == L'\0')
        return 301;
    if (str[1] == L'/')
        return 0;
    if (wcspbrk(str + 1, L":="))
        return 0;

    if (wcschr(str + 1, L'/')) {
        while (pathmatches[i] != NULL) {
            if (xwcsmatch(str, pathmatches[i]) == 0)
                return i + 100;
            i++;
        }
        if (xforcewp)
            return 150;
    }
    else {
        while (pathfixed[i] != NULL) {
            if (wcscmp(str, pathfixed[i]) == 0)
                return i + 200;
            i++;
        }
        if (xforcewp)
            return 250;
    }
    return 0;
}

static int isanypath(const wchar_t *s)
{
    int r;

    if (IS_EMPTY_WCS(s))
        return 0;
    switch (s[0]) {
        case L'"':
        case L'\'':
            r = 0;
        break;
        case L'/':
            r = 1;
        break;
        case L'.':
            r = isdotpath(s);
        break;
        default:
            r = iswinpath(s);
        break;
    }
    return r;
}

/**
 * If argument starts with '[--]name=' the
 * function will return the string after '='.
 */
static wchar_t *cmdoptionval(wchar_t *s)
{
    int n = 0;

    if ((s[0] == L'-') && (s[1] == L'-'))
        s += 2;
    if ((s[0] == L'-'))
        return NULL;
    while (*s != L'\0') {
        int c = *(s++);
        if (n > 0) {
            if (c == L'=')
                return s;
        }
        if (xisvarchar(c))
            n++;
        else
            break;
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

static void rmendingspaces(wchar_t *s)
{
    int i = (int)xwcslen(s) - 1;

    while (i > 1) {
        if ((s[i] == L' ') || (s[i] == L'\t'))
            s[i--] = L'\0';
        else
            break;
    }
    if (i > 1) {
        if ((s[i] == L'.') && IS_PSW(s[i-1]))
            s[i] = L'\0';
    }
}

static void collapsefwpsep(wchar_t *s)
{
    int n;
    int c;
    int i = (int)xwcslen(s);

    for (n = 1, c = 1; n < i; n++) {
        if (s[n] == L'/')  {
            if (s[n + 1] == L'/') {
                continue;
            }
        }

        s[c++] = s[n];
    }
    if (c < n)
        s[c] = L'\0';
    rmendingspaces(s);
}

/**
 * Remove trailing backslash and path separator(s)
 * so that we don't have problems with quoting
 * or appending
 */
static void rmtrailingpsep(wchar_t *s)
{
    int i = (int)xwcslen(s) - 1;

    while (i > 1) {
        if ((s[i] == L';') || (s[i] == L' ') || (s[i] == L'\t'))
            s[i--] = L'\0';
        else
            break;
    }
    while (i > 1) {
        if (IS_PSW(s[i]) && (s[i-1] != L'.'))
            s[i--] = L'\0';
        else
            break;
    }
    if (i > 1) {
        if ((s[i] == L'.') && IS_PSW(s[i-1]))
            s[i] = L'\0';
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
    *tokens = 0;
    while ((e = wcschr(s, L':')) != NULL) {
        wchar_t *p;
        size_t   n = (size_t)(e - s);

        if (n == 0) {
            /**
             * Handle multiple collons
             */
            s = e + 1;
            continue;
        }
        p = xwalloc(n + 2);
        wmemcpy(p, s, n);
        sa[c++] = p;
        if (isposixpath(p)) {
            s = e + 1;
        }
        else {
            /**
             * Token before ':' was not a posix path.
             * Return original str regerdless if some
             * previous tokens evaluated as posix path.
             */
            waafree(sa);
            return NULL;
        }
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

static wchar_t *posixtowin(wchar_t *pp, int m)
{
    wchar_t *rv = NULL;
    wchar_t  windrive[] = { L'\0', L':', L'\\', L'\0'};

    if (m == 0)
        m = isposixpath(pp);
    if (m == 0) {
        /**
         * Not a posix path
         */
        return pp;
    }
    else if (m == 100) {
        /**
         * /cygdrive/x/... absolute path
         */
        windrive[0] = towupper(pp[10]);
        rv = xwcsconcat(windrive, pp + 12);
        collapsefwpsep(rv + 3);
        replacepathsep(rv + 3);
    }
    else if (m == 102) {
        /**
         * For anything from /dev/* return \\.\NUL
         */
        rv = xwcsdup(L"\\\\.\\NUL");
    }
    else if (m == 300) {
        collapsefwpsep(pp);
        replacepathsep(pp);
        return pp;
    }
    else if (m == 301) {
        rv = xwcsdup(posixroot);
    }
    else {
        collapsefwpsep(pp);
        replacepathsep(pp);
        rv = xwcsconcat(posixroot, pp);
    }
    xfree(pp);
    return rv;
}

static wchar_t *pathtowin(wchar_t *pp)
{
    wchar_t *rv = pp;

    if (iswinpath(pp)) {
        rmendingspaces(pp);
        replacepathsep(pp);
    }
    else {
        rv = posixtowin(pp, 0);
    }
    return rv;
}

static wchar_t *towinpath(const wchar_t *str)
{
    wchar_t *wp = xwcsdup(str);

    if (iswinpath(wp)) {
        rmendingspaces(wp);
        replacepathsep(wp);
    }
    else if (xwcsmatch(wp, L"*/+*:*/+*") == 0) {
        /**
         * We have [...]/x[...]:[...]/x[...]
         * which can be eventually converted
         * to token1;token2[;tokenN...]
         * If any of tokens is not a posix path
         * return original string.
         */
        int i, n;
        wchar_t **pa = splitpath(wp, &n);

        if (pa != NULL) {
            for (i = 0; i < n; i++) {
                pa[i] = posixtowin(pa[i], 0);
            }
            xfree(wp);
            wp = mergepath(pa);
            waafree(pa);
        }
    }
    else {
        wp = posixtowin(wp, 0);
    }
    return wp;
}

static wchar_t *getposixroot(wchar_t *r)
{

    if (r == NULL) {
        const wchar_t **e = posixrenv;

        while (*e != NULL) {
            r = xgetenv(*e);
            if (IS_VALID_WCS(r))
                break;
            e++;
        }
        if (r == NULL) {
            r = xgetenv(L"HOME");
            if (IS_VALID_WCS(r)) {
                wchar_t *p = wcsstr(r, L"\\home\\");
                if (IS_EMPTY_WCS(p)) {
                    xfree(r);
                    r = NULL;
                }
                else {
                    *p = L'\0';
                    return r;
                }
            }
        }
        if (r == NULL) {
            /**
             * Use default locations
             */
            r = xwcsdup(L"C:\\cygwin64");
            if (_waccess(r, 0)) {
                r[9] = L'\0';
                if (_waccess(r, 0)) {
                    xfree(r);
                    r = NULL;
                }
            }
            return r;
        }
    }
    replacepathsep(r);
    rmtrailingpsep(r);
    r[0] = towupper(r[0]);
    return r;
}

static wchar_t *getrealpathname(const wchar_t *path)
{
    wchar_t    *buf  = NULL;
    DWORD       siz  = _MAX_FNAME;
    DWORD       len  = 0;
    HANDLE      fh;

    if (IS_EMPTY_WCS(path))
        return NULL;
    fh = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                     OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (IS_INVALID_HANDLE(fh))
        return NULL;

    while (buf == NULL) {
        buf = xwalloc(siz);
        len = GetFinalPathNameByHandleW(fh, buf, siz, VOLUME_NAME_DOS);
        if (len == 0) {
            CloseHandle(fh);
            xfree(buf);
            return NULL;
        }
        if (len > siz) {
            xfree(buf);
            buf = NULL;
            siz = len;
        }
    }
    CloseHandle(fh);
    if ((len > 5) && (len < _MAX_FNAME)) {
        /**
         * Strip leading \\?\ for short paths
         * but not \\?\UNC\* paths
         */
        if ((buf[0] == L'\\') &&
            (buf[1] == L'\\') &&
            (buf[2] == L'?')  &&
            (buf[3] == L'\\') &&
            (buf[5] == L':')) {
            wmemmove(buf, buf + 4, len - 3);
        }
    }
    return buf;
}

static wchar_t *xsearchexe(const wchar_t *paths, const wchar_t *name)
{
    wchar_t  *sp = NULL;
    wchar_t  *rp = NULL;
    DWORD     ln = 0;
    DWORD     sz = _MAX_PATH;

    while (sp == NULL) {
        sp = xwalloc(sz);
        ln = SearchPathW(paths, name, L".exe", sz, sp, NULL);
        if (ln == 0) {
            xfree(sp);
            return NULL;
        }
        else if (ln > sz) {
            xfree(sp);
            sp = NULL;
            sz = ln;
        }
    }
    rp = getrealpathname(sp);
    xfree(sp);
    return rp;
}

static wchar_t *realappname(const wchar_t *path)
{
    int i;
    const wchar_t *p = path;

    i = (int)xwcslen(path);
    while (--i > 0) {
        if (IS_PSW(path[i])) {
            /** Found path separator */
            p = path + i + 1;
            break;
        }
    }
    if ((p[0] == L'_') && (p[1] == L'_'))
        return xwcsdup(p + 2);
    else
        return NULL;
}

static BOOL WINAPI consolehandler(DWORD ctrl)
{
    return TRUE;
}

static int posixmain(int argc, wchar_t **wargv, int envc, wchar_t **wenvp)
{
    int i, rc = 0;
    intptr_t rp;

    if (xdumpenv == 0) {
        for (i = xrunexec; i < argc; i++) {
            wchar_t *a = wargv[i];

            if ((a[0] == L'"') || (a[0] == L'\'')) {
                /**
                 * Skip quoted argument
                 */
            }
            else if (iswinpath(a)) {
                rmendingspaces(a);
                replacepathsep(a);
            }
            else {
                int m = isposixpath(a);

                if (m != 0) {
                    /**
                     * We have posix path
                     */
                     wargv[i] = posixtowin(a, m);
                }
                else {
                    wchar_t *v = cmdoptionval(a);

                    if (isanypath(v)) {
                        if (iswinpath(v)) {
                            rmendingspaces(v);
                            replacepathsep(v);
                        }
                        else {
                            m = isposixpath(v);
                            if (m != 0) {
                                wchar_t *p = xwcsdup(v);

                                p = posixtowin(p, m);
                                v[0] = L'\0';
                                wargv[i] = xwcsconcat(a, p);
                                xfree(p);
                                xfree(a);
                            }
                        }
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
        for (i = 0; i < (envc - 3); i++) {
            wchar_t *v;
            wchar_t *p;
            wchar_t *e = wenvp[i];

            v = wcschr(e, L'=');
            if ((v == NULL) || (v[1] == L'\0')) {
                /**
                 * Bad environment
                 */
                return EBADF;
            }
            v++;
            if (isanypath(v)) {
                p = towinpath(v);

                v[0] = L'\0';
                wenvp[i] = xwcsconcat(e, p);
                xfree(e);
                xfree(p);
            }
        }
    }
    qsort((void *)wenvp, envc, sizeof(wchar_t *), envsort);
    if (xdumpenv) {
        int x = 0;

        if (argc > 0) {
            int n;
            for (n = 0; n < argc; n++) {
                for (i = 0; i < envc; i++) {
                    if (xwcsisenvvar(wenvp[i], wargv[n])) {
                        if (x++ > 0)
                            fputwc(L'\n', stdout);
                        fputws(wenvp[i], stdout);
                        break;
                    }
                }
                if (i == envc) {
                    if (x > 0)
                        fputwc(L'\n', stderr);
                    fputws(L"Error: Environment variable '", stderr);
                    fputws(wargv[n], stderr);
                    fputws(L"' cannot be found", stderr);
                    break;
                }
            }
        }
        else {
            for (i = 0; i < envc; i++) {
                if (i > 0)
                    fputwc(L'\n', stdout);
                fputws(wenvp[i], stdout);
            }
        }
        if (argc == x)
            return 0;
        else
            return ENOENT;
    }
    _flushall();
    rp = _wspawnvpe(_P_NOWAIT, wargv[0], wargv, wenvp);
    if (rp == (intptr_t)-1) {
        rc = errno;
        if (xshowerr)
            fwprintf(stderr, L"Failed to execute: '%s'\n", wargv[0]);
    }
    else {
        /**
         * wait for child to exit
         */
        SetConsoleCtrlHandler(consolehandler, TRUE);
        if (_cwait(&rc, rp, 0) == (intptr_t)-1) {
            rc = errno;
            if (xshowerr) {
                fwprintf(stderr, L"Execute failed: %s\nFatal error: %s\n\n",
                         wargv[0], _wcserror(rc));
            }
        }
        SetConsoleCtrlHandler(consolehandler, FALSE);
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

    int dupenvc = 0;
    int dupargc = 0;
    int clreenv = 1;
    int envc    = 0;
    int opts    = 1;
    int haseopt = 0;
    int haspopt = 0;


    if (wenv == NULL) {
        fputs("\nMissing environment\n", stderr);
        return EBADF;
    }
    dupwargv = waalloc(argc);
    dupwargv[0] = realappname(wargv[0]);

    if (dupwargv[0] != NULL) {
        dupargc = 1;
        opts    = 0;
    }
    for (i = 1; i < argc; i++) {
        const wchar_t *p = wargv[i];

        if (opts) {
            /**
             * Simple argument parsing
             */
            if (cwd == zerostring) {
                cwd = xwcsdup(p);
                continue;
            }
            if (crp == zerostring) {
                crp = xwcsdup(p);
                continue;
            }

            if (p[0] == L'-') {

                if ((p[1] == L'\0') || (p[2] != L'\0')) {
                    if (haspopt) {
                        dupwargv[dupargc++] = xwcsdup(p);
                        opts = 0;
                        continue;
                    }
                    return invalidarg(p);
                }
                switch (p[1]) {
                    case L'k':
                        clreenv  = 0;
                    break;
                    case L'e':
                        xdumpenv = 1;
                        xrunexec = 0;
                        haseopt  = 1;
                    break;
                    case L'f':
                        xforcewp = 1;
                    break;
                    case L'p':
                        xrunexec = 0;
                        xdumpenv = 0;
                        haspopt  = 1;
                    break;
                    case L'r':
                        crp = zerostring;
                    break;
                    case L's':
                        xskipenv = 1;
                    break;
                    case L'q':
                        xshowerr = 0;
                    break;
                    case L'w':
                        cwd = zerostring;
                    break;
                    case L'v':
                        return version(0);
                    break;
                    case L'V':
                        return version(1);
                    break;
                    case L'h':
                    case L'?':
                        xshowerr = 1;
                        return usage(0);
                    break;
                    default:
                        opts = 0;
                        if (haspopt)
                            dupwargv[dupargc++] = xwcsdup(p);
                        else
                            return invalidarg(p);
                    break;
                }
                continue;
            }
            opts = 0;
        }
        dupwargv[dupargc++] = xwcsdup(p);
    }
    if (haseopt && haspopt) {
        if (xshowerr)
            fputs("Cannot have both -p and -e options defined\n", stderr);
        return usage(EINVAL);
    }
    if ((dupargc == 0) && (xdumpenv == 0)) {
        if (xshowerr)
            fputs("Missing PROGRAM argument\n", stderr);
        return usage(1);
    }
    if ((cwd == zerostring) || (crp == zerostring)) {
        if (xshowerr)
            fputs("Missing required parameter value\n", stderr);
        return usage(1);
    }
    cpp = xgetenv(L"PATH");
    if (IS_EMPTY_WCS(cpp)) {
        if (xshowerr)
            fputs("Missing PATH environment variable\n", stderr);
        return ENOENT;
    }
    rmtrailingpsep(cpp);

    posixroot = getposixroot(crp);
    if (IS_EMPTY_WCS(posixroot)) {
        if (xshowerr)
            fputs("Cannot find valid POSIX_ROOT\n", stderr);
        return ENOENT;
    }
    if (cwd != NULL) {
        cwd = pathtowin(cwd);
        if (_wchdir(cwd) != 0) {
            if (xshowerr)
                fwprintf(stderr, L"Invalid working directory: '%s'\n", cwd);
            return ENOENT;
        }
        xfree(cwd);
    }
    cwd = _wgetcwd(NULL, 0);
    if (cwd == NULL) {
        if (xshowerr)
            fputs("Cannot get current working directory\n", stderr);
        return ENOENT;
    }
    while (wenv[envc] != NULL) {
        ++envc;
    }

    dupwenvp = waalloc(envc + 4);
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
        if (p != NULL) {
            dupwenvp[dupenvc++] = xwcsdup(p);
        }
    }
    if (xrunexec) {
        wchar_t *sch;
        wchar_t *exe = pathtowin(dupwargv[0]);

        sch = getrealpathname(exe);
        if (sch == NULL) {
            wchar_t *pps;
            /**
             * PROGRAM is not an absolute path.
             * Add current directory as first PATH element
             * and search for PROGRAM[.exe]
             */
            if (isdotpath(exe))
                pps = xwcscpaths(cwd, cpp);
            else
                pps = cpp;
            sch = xsearchexe(pps, exe);
            if (sch == NULL) {
                if (xshowerr)
                    fwprintf(stderr, L"Cannot find PROGRAM '%s' inside %s\n",
                             exe, pps);
                return ENOENT;
            }
            if (pps != cpp)
                xfree(pps);
        }
        xfree(exe);
        dupwargv[0] = sch;
    }
    /**
     * Add back environment variables
     */
    dupwenvp[dupenvc++] = xwcsconcat(L"PATH=", cpp);
    dupwenvp[dupenvc++] = xwcsconcat(L"PWD=",  cwd);
    dupwenvp[dupenvc++] = xwcsconcat(L"POSIX_ROOT=", posixroot);
    xfree(cpp);
    xfree(cwd);
    return posixmain(dupargc, dupwargv, dupenvc, dupwenvp);
}
