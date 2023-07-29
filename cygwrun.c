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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <errno.h>
#include <process.h>
#include <fcntl.h>
#include <io.h>
#include "cygwrun.h"

#define WNUL              L'\0'
#define IS_PSW(_c)        (((_c) == L'/') || ((_c)  == L'\\'))
#define IS_EMPTY_WCS(_s)  (((_s) == NULL) || (*(_s) == WNUL))
#define IS_VALID_WCS(_s)  (((_s) != NULL) && (*(_s) != WNUL))
#define BBUFSIZ           512
#define SBUFSIZ           1024
#define MBUFSIZ           2048
#define HBUFSIZ           8192
#define EBUFSIZ           32768

static int      xrunexec  = 1;
static int      xdumparg  = 0;
static int      xshowerr  = 1;
static int      xforcewp  = 0;
static int      xrmendps  = 1;
static int      xwoptind  = 1;   /* Index into parent argv vector */
static int      xwoption  = 0;   /* Character checked for validity */
static const wchar_t  *xwoptarg = NULL;

static wchar_t *posixroot = NULL;
static wchar_t **xrawenvs = NULL;

static const wchar_t *pathmatches[] = {
    L"/cygdrive/?/+*",
    L"/clang64/*",
    L"/bin/*",
    L"/dev/*",
    L"/etc/*",
    L"/home/*",
    L"/lib/*",
    L"/lib64/*",
    L"/mingw64/*",
    L"/opt/*",
    L"/root/*",
    L"/run/*",
    L"/sbin/*",
    L"/share/*",
    L"/tmp/*",
    L"/ucrt64/*",
    L"/usr/*",
    L"/var/*",
    NULL
};

static const wchar_t *pathfixed[] = {
    L"/bin",
    L"/clang64",
    L"/dev",
    L"/etc",
    L"/home",
    L"/lib",
    L"/lib64",
    L"/mingw64",
    L"/opt",
    L"/root",
    L"/run",
    L"/sbin",
    L"/share",
    L"/tmp",
    L"/ucrt64",
    L"/usr",
    L"/var",
    NULL
};

static const wchar_t *specialenv = L"!::,!;,PS1,ORIGINAL_TEMP,ORIGINAL_TMP";

static const wchar_t *removeenv[] = {
    L"_",
    L"CYGWIN_ROOT",
    L"OLDPWD",
    L"ORIGINAL_PATH",
    L"PATH",
    L"POSIX_PATH",
    L"POSIX_ROOT",
    L"PWD",
    L"TEMP",
    L"TMP",
    NULL
};

static const wchar_t *posixrenv[] = {
    L"POSIX_ROOT",
    L"CYGWIN_ROOT",
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

static wchar_t zerostring[4] = { WNUL, WNUL, WNUL, WNUL };

static int usage(int rv)
{
    if (xshowerr) {
        FILE *os = rv == 0 ? stdout : stderr;
        fputs("\nUsage " PROJECT_NAME " [OPTIONS]... PROGRAM [ARGUMENTS]...\n", os);
        fputs("Execute PROGRAM [ARGUMENTS]...\n\nOptions are:\n", os);
        fputs(" -r <DIR>  use DIR as posix root\n", os);
        fputs(" -w <DIR>  change working directory to DIR before calling PROGRAM\n", os);
        fputs(" -n <ENV>  do not translate ENV variable(s)\n", os);
        fputs("           multiple variables are comma separated.\n", os);
        fputs(" -f        convert all unknown posix absolute paths.\n", os);
        fputs(" -K        keep trailing path separators for paths.\n", os);
        fputs(" -q        do not print errors to stderr.\n", os);
        fputs(" -v        print version information and exit.\n", os);
        fputs(" -V        print detailed version information and exit.\n", os);
        fputs(" -h | -?   print this screen and exit.\n", os);
        fputs(" -p        print arguments instead executing PROGRAM.\n\n", os);
        fputs("To file bugs, visit " PROJECT_URL, os);
    }
    return rv;
}

static int version(int verbose)
{
    if (verbose) {
        fputs(PROJECT_NAME " version " PROJECT_VERSION_ALL "\n\n", stdout);
        fputs(PROJECT_LICENSE_SHORT "\n\n", stdout);
        fputs("Visit " PROJECT_URL " for more details", stdout);
    }
    else {
        fputs(PROJECT_NAME " version " PROJECT_VERSION_TXT, stdout);
    }
    return 0;
}


static wchar_t *xwmalloc(size_t size)
{
    wchar_t *p = (wchar_t *)malloc((size + 2) * sizeof(wchar_t));
    if (p == NULL) {
        _wperror(L"xwmalloc");
        _exit(1);
    }
    p[size++] = WNUL;
    p[size]   = WNUL;
    return p;
}

static void *xcalloc(size_t number, size_t size)
{
    void *p = calloc(number + 2, size);
    if (p == NULL) {
        _wperror(L"xcalloc");
        _exit(1);
    }
    return p;
}

static wchar_t **waalloc(size_t size)
{
    return (wchar_t **)xcalloc(size, sizeof(wchar_t *));

    wchar_t **p = (wchar_t **)calloc(size + 2, sizeof(wchar_t *));
    if (p == NULL) {
        _wperror(L"waalloc");
        _exit(1);
    }
    return p;
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
    p = xwmalloc(n);
    return wmemcpy(p, s, n);
}

static wchar_t *xwcsndup(const wchar_t *s, size_t len)
{
    wchar_t *p;
    size_t   n;

    if (IS_EMPTY_WCS(s) || (len == 0))
        return NULL;
    n = wcsnlen(s, len);
    p = xwmalloc(n);
    return wmemcpy(p, s, n);
}

static wchar_t *xgetenv(const wchar_t *s)
{
    DWORD    n;
    wchar_t  e[BBUFSIZ];
    wchar_t *d = NULL;

    if (IS_EMPTY_WCS(s))
        return NULL;
    n =  GetEnvironmentVariableW(s, e, BBUFSIZ);
    if (n != 0) {
        d = xwmalloc(n);
        if (n > BBUFSIZ) {
            GetEnvironmentVariableW(s, d, n);
        }
        else {
            wmemcpy(d, e, n);
        }
    }
    return d;
}

static wchar_t *xgetcwd(void)
{
    DWORD    n;
    wchar_t  e[BBUFSIZ];
    wchar_t *d = NULL;

    n =  GetCurrentDirectoryW(BBUFSIZ, e);
    if (n != 0) {
        d = xwmalloc(n);
        if (n > BBUFSIZ) {
            GetCurrentDirectoryW(n, d);
        }
        else {
            wmemcpy(d, e, n);
        }
    }
    return d;
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
    wchar_t *rv;
    size_t l1;
    size_t l2;

    l1 = xwcslen(s1);
    l2 = xwcslen(s2);

    if ((l1 + l2) == 0)
        return NULL;
    rv = xwmalloc(l1 + l2);

    if(l1 > 0)
        wmemcpy(rv, s1, l1);
    if(l2 > 0)
        wmemcpy(rv + l1, s2, l2);
    return rv;
}

static wchar_t *xwcsappend(wchar_t *s, wchar_t ch, const wchar_t *a)
{
    wchar_t *p;
    wchar_t *e;
    size_t   n = xwcslen(s);
    size_t   x = xwcslen(a);

    p = xwmalloc(n + x + 1);
    e = p;

    if (n > 0) {
        wmemcpy(e, s, n);
        e += n;
    }
    *(e++) = ch;
    if (x > 0) {
        wmemcpy(e, a, x);
        e += x;
    }
    *e = WNUL;
    xfree(s);
    return p;
}

/**
 * Remove trailing spaces and return a pointer
 * to the first non white space character
 */
static wchar_t *xwcstrim(wchar_t *s)
{
    size_t i = xwcslen(s);

    while (i > 0) {
        i--;
        if (s[i] < L'!')
            s[i] = WNUL;
        else
            break;
    }
    if (i > 0) {
        while ((*s > WNUL) && (*s < L'!'))
            s++;
    }
    return s;
}

static int xisvarchar(int c)
{
    if ((c < 32) || (c > 127))
        return 0;
    else
        return xvalidvarname[c];

}

/**
 * Match = 0, NoMatch = 1, Abort = -1
 * Based loosely on sections of wildmat.c by Rich Salz
 */
static int xwcsmatch(const wchar_t *wstr, const wchar_t *wexp)
{
    int match;
    int mflag;

    for ( ; *wexp != WNUL; wstr++, wexp++) {
        if (*wstr == WNUL && *wexp != L'*')
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
                if (*wexp == WNUL)
                    return 0;
                while (*wstr != WNUL) {
                    int rv = xwcsmatch(wstr++, wexp);
                    if (rv != 1)
                        return rv;
                }
                return -1;
            break;
            case L'?':
                if (xisvarchar(*wstr) != 2)
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
                    if ((*wexp == WNUL) || (*wexp == L'['))
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
    return (*wstr != WNUL);
}

/**
 * Count the number of tokens delimited by d
 */
static int xwcsntok(const wchar_t *s, wchar_t d)
{
    int n = 1;

    while (*s == d) {
        s++;
    }
    if (*s == WNUL)
        return 0;
    while (*s != WNUL) {
        if (*(s++) == d) {
            while (*s == d) {
                s++;
            }
            if (*s != WNUL)
                n++;
        }
    }
    return n;
}

/**
 * This is wcstok_s clone using single character as token delimiter
 */
static wchar_t *xwcsctok(wchar_t *s, wchar_t d, wchar_t **c)
{
    wchar_t *p;

    if ((s == NULL) && ((s = *c) == NULL))
        return NULL;

    *c = NULL;
    /**
     * Skip leading delimiter
     */
    while (*s == d) {
        s++;
    }
    if (*s == WNUL)
        return NULL;
    p = s;

    while (*s != WNUL) {
        if (*s == d) {
            *s = WNUL;
            *c = s + 1;
            break;
        }
        s++;
    }
    return p;
}

/**
 * Quote one command line argument if needed. See documentation for
 * CommandLineToArgV() for details:
 * https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-commandlinetoargvw
 */
static wchar_t *xquotearg(wchar_t *s)
{
    size_t   n = 0;
    wchar_t *c;
    wchar_t *e;
    wchar_t *d;

    /* Perform quoting only if neccessary. */
    if (IS_EMPTY_WCS(s))
        return s;
    if (wcspbrk(s, L" \f\n\r\t\v\"") == NULL)
        return s;

    for (c = s; ; c++) {
        size_t b = 0;

        while (*c == L'\\') {
            b++;
            c++;
        }

        if (*c == WNUL) {
            /* Escape backslashes. */
            n += b * 2;
            break;
        }
        else if (*c == L'"') {
            /* Escape backslashes. */
            n += b * 2 + 1;
            /* Double quote char. */
            n += 1;
        }
        else {
            /* Unescaped backslashes. */
            n += b;
            /* Original character. */
            n += 1;
        }
    }
    /* For quotes */
    n += 2;
    e = xwmalloc(n);
    d = e;

    *(d++) = L'"';
    for (c = s; ; c++) {
        size_t b = 0;

        while (*c == '\\') {
            b++;
            c++;
        }

        if (*c == WNUL) {
            wmemset(d, L'\\', b * 2);
            d += b * 2;
            break;
        }
        else if (*c == L'"') {
            wmemset(d, L'\\', b * 2 + 1);
            d += b * 2 + 1;
            *(d++) = *c;
        }
        else {
            wmemset(d, L'\\', b);
            d += b;
            *(d++) = *c;
        }
    }
    xfree(s);

    *(d++) = L'"';
    *(d)   = WNUL;

    return e;
}

int xwgetopt(int nargc, const wchar_t **nargv, const wchar_t *opts)
{
    const wchar_t *oli = NULL;
    static const wchar_t *place = zerostring;

    xwoptarg = NULL;
    if (*place == WNUL) {
        if (xwoptind >= nargc) {
            /* No more arguments */
            place = zerostring;
            return EOF;
        }
        place = nargv[xwoptind];
        if (*(place++) != L'-') {
            /* Argument is not an option */
            place = zerostring;
            return EOF;
        }
        if ((*place == WNUL) || (*place == L'-')) {
            if (*place == '-')
                place++;
            if (*place == WNUL)
                xwoptind++;
            place = zerostring;
            return EOF;
        }
    }

    xwoption = *(place++);
    if (xwoption != L':') {
        oli = wcschr(opts, (wchar_t)xwoption);
    }
    if (oli == NULL) {
        xwoptind++;
        place = zerostring;
        return EINVAL;
    }
    /* Does this option need an argument? */
    if (oli[1] == L':') {
        /*
         * Option-argument is either the rest of this argument
         * or the entire next argument.
         */
        if (*place) {
            /* Skip blanks */
            while (iswblank(*place))
                ++place;
        }
        if (*place != WNUL) {
            xwoptarg = place;
        }
        else if (nargc > ++xwoptind) {
            xwoptarg = nargv[xwoptind];
        }
        ++xwoptind;
        place = zerostring;
        if (IS_EMPTY_WCS(xwoptarg)) {
            /* Option-argument is absent or empty */
            return ENOENT;
        }
    }
    else {
        /* Don't need argument */
        if (*place == WNUL) {
            ++xwoptind;
            place = zerostring;
        }
    }
    return oli[0];
}

static int xwcsisenvvar(const wchar_t *str, const wchar_t *var, int icase)
{
    while (*str != WNUL) {
        wchar_t cs = icase ? towupper(*str) : *str;
        wchar_t cv = icase ? towupper(*var) : *var;

        if (cs != cv)
            break;
        str++;
        var++;
        if (*var == WNUL)
            return *str == L'=';
    }
    return 0;
}

static int xisbatchfile(const wchar_t *s)
{
    size_t n = xwcslen(s);
    /* a.bat */
    if (n > 4) {
        const wchar_t *e = s + n - 4;

        if (*(e++) == L'.') {
            if (_wcsicmp(e, L"bat") == 0)
                return 1;
            if (_wcsicmp(e, L"cmd") == 0)
                return 1;
        }
    }
    return 0;
}

static int iswinpath(const wchar_t *s)
{
    int i = 0;

    if (IS_PSW(s[0])) {
        if (IS_PSW(s[1]) && (s[2] != WNUL)) {
            i  = 2;
            s += 2;
            if ((s[0] == L'?' ) &&
                (IS_PSW(s[1]) ) &&
                (s[2] != WNUL)) {
                /**
                 * We have \\?\* path
                 */
                i += 2;
                s += 2;
            }
        }
    }
    if (xisvarchar(s[0]) == 2) {
        if (s[1] == L':') {
            if (s[2] == WNUL)
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
    if (str[1] == WNUL)
        return 301;
    if (str[1] == L'/')
        return iswinpath(str);
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
        case L'\'':
            r = 0;
        break;
        case L'/':
            r = isposixpath(s);
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

static int isrelativepath(const wchar_t *p)
{
    if (IS_EMPTY_WCS(p))
        return 0;
    if (p[0] < 127) {
        if (IS_PSW(p[0]) || (isalpha(p[0]) && (p[1] == L':')))
            return 0;
    }
    return 1;
}


/**
 * If argument starts with '[--]name=' the
 * function will return the string after '='.
 */
static wchar_t *cmdoptionval(wchar_t *s)
{
    int n = 0;

    if (IS_EMPTY_WCS(s))
        return NULL;

    while (*s != WNUL) {
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

static wchar_t *xarrblk(int cnt, const wchar_t **arr, wchar_t sep)
{
    int      i;
    size_t   len = 0;
    size_t  *ssz;
    wchar_t *ep;
    wchar_t *bp;

    ssz = (size_t *)xcalloc(cnt, sizeof(size_t));
    for (i = 0; i < cnt; i++) {
        size_t n = xwcslen(arr[i]);
        ssz[i]   = n++;
        len     += n;
    }

    bp = xwmalloc(len + 2);
    ep = bp;
    for (i = 0; i < cnt; i++) {
        size_t n = ssz[i];
        if (i > 0)
            *(ep++) = sep;
        wmemcpy(ep, arr[i], n);
        ep += n;
    }
    xfree(ssz);
    ep[0] = WNUL;
    ep[1] = WNUL;

    return bp;
}

static wchar_t *cleanpath(wchar_t *s)
{
    int n = 0;
    int c;
    int i;


    i = (int)xwcslen(s);
    if (i == 0)
        return s;
    if (i > 2) {
        if ((s[0] < 127) && isalpha(s[0]) && (s[1] == L':') && IS_PSW(s[2])) {
            s[0] = towupper(s[0]);
            s[2] = L'\\';
            n = 3;
        }
    }
    for (c = n; n < i; n++) {
        if (IS_PSW(s[n]))  {
            if ((n > 0) && IS_PSW(s[n + 1])) {
                continue;
            }
            s[n] = L'\\';
        }

        s[c++] = s[n];
    }
    s[c--] = WNUL;
    while (c > 0) {
        if ((s[c] == L';') || (s[c] < L'!'))
            s[c--] = WNUL;
        else
            break;
    }
    if (xrmendps) {
        while (c > 1) {
            if ((s[c] == L'\\') && (s[c - 1] != L'.'))
                s[c--] = WNUL;
            else
                break;
        }
    }
    return s;
}

static wchar_t **xsplitstr(const wchar_t *s, wchar_t sc)
{
    int      c, x = 0;
    wchar_t  *cx = NULL;
    wchar_t  *ws;
    wchar_t  *es;
    wchar_t **sa;

    c =  xwcsntok(s, sc);
    if (c == 0)
        return NULL;
    ws = xwcsdup(s);
    sa = waalloc(c);
    es = xwcsctok(ws, sc, &cx);
    while (es != NULL) {
        es = xwcstrim(es);
        if (*es != WNUL)
            sa[x++] = xwcsdup(es);
        es = xwcsctok(NULL, sc, &cx);
    }
    xfree(ws);
    return sa;
}

static wchar_t **splitpath(const wchar_t *s, wchar_t ps)
{
    int      c, x = 0;
    wchar_t  *ws;
    wchar_t **sa;
    wchar_t  *cx = NULL;
    wchar_t  *es;

    c =  xwcsntok(s, ps);
    if (c == 0)
        return NULL;
    ws = xwcsdup(s);
    sa = waalloc(c);

    es = xwcsctok(ws, ps, &cx);
    while (es != NULL) {
        es = xwcstrim(es);
        if (*es != WNUL)
            sa[x++] = xwcsdup(es);
        es = xwcsctok(NULL, ps, &cx);
    }
    xfree(ws);
    return sa;
}

static wchar_t *mergepath(const wchar_t **pp)
{
    int  i, x;
    size_t s[64];
    size_t n, len = 0;
    wchar_t  *r, *p;

    for (i = 0, x = 0; pp[i] != NULL; i++, x++) {
        n = wcslen(pp[i]);
        if (x < 64)
            s[x] = n;
        len += (n + 1);
    }
    r = p = xwmalloc(len + 2);
    for (i = 0, x = 0; pp[i] != NULL; i++, x++) {
        if (x < 64)
            n = s[x];
        else
            n = wcslen(pp[i]);
        if (i > 0)
            *(p++) = L';';
        wmemcpy(p, pp[i], n);
        p += n;
    }
    *p = WNUL;
    return r;
}

static wchar_t *posixtowin(wchar_t *pp, int m)
{
    wchar_t *rv = NULL;
    wchar_t  windrive[] = { WNUL, L':', L'\\', WNUL};

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
        cleanpath(rv + 3);
    }
    else if (m == 102) {
        /**
         * For anything from /dev/ return \\.\NUL
         */
        rv = xwcsdup(L"\\\\.\\NUL");
    }
    else if (m == 300) {
        return cleanpath(pp);
    }
    else if (m == 301) {
        rv = xwcsdup(posixroot);
    }
    else {
        cleanpath(pp);
        rv = xwcsconcat(posixroot, pp);
    }
    xfree(pp);
    return rv;
}

static wchar_t *pathtowin(wchar_t *pp)
{
    int m;

    m = isanypath(pp);
    if (m == 0)
        return pp;
    if (m < 100) {
        return cleanpath(pp);
    }
    else {
        return posixtowin(pp, m);
    }
}

static wchar_t *towinpaths(const wchar_t *ps, int m)
{
    int i;
    int sc = 0;
    wchar_t **pa;
    wchar_t  *wp = NULL;

    if (m < 100) {
        if (m == 0) {
            if (wcschr(ps, L';') || iswinpath(ps))
                sc = 1;
        }
        else {
            sc = 1;
        }
    }
    if (sc) {
        pa = splitpath(ps, L';');

        if (pa != NULL) {
            for (i = 0; pa[i] != NULL; i++) {
                cleanpath(pa[i]);
            }
            wp = mergepath(pa);
            waafree(pa);
        }
    }
    else {
        pa = splitpath(ps, L':');

        if (pa != NULL) {
            for (i = 0; pa[i] != NULL; i++) {
                m = isposixpath(pa[i]);
                if (m == 0) {
                    waafree(pa);
                    return xwcsdup(ps);
                }
                else {
                    pa[i] = posixtowin(pa[i], m);
                }
            }
            wp = mergepath(pa);
            waafree(pa);
        }
    }
    if (wp == NULL)
        wp = xwcsdup(ps);
    return wp;
}


static wchar_t *getrealpathname(const wchar_t *path, int isdir)
{
    wchar_t    *buf  = NULL;
    DWORD       siz  = MAX_PATH;
    DWORD       len  = 0;
    HANDLE      fh;
    DWORD       fa   = isdir ? FILE_FLAG_BACKUP_SEMANTICS : FILE_ATTRIBUTE_NORMAL;

    if (IS_EMPTY_WCS(path))
        return NULL;
    fh = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                     OPEN_EXISTING, fa, NULL);
    if (IS_INVALID_HANDLE(fh))
        return NULL;

    while (buf == NULL) {
        buf = xwmalloc(siz);
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
    DWORD     sz = MAX_PATH;

    while (sp == NULL) {
        sp = xwmalloc(sz);
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
    rp = getrealpathname(sp, 0);
    xfree(sp);
    return rp;
}

static wchar_t *getparentname(void)
{
    DWORD  pid;
    HANDLE h;
    HANDLE p;
    wchar_t *n = NULL;
    PROCESSENTRY32W e;

    h = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (IS_INVALID_HANDLE(h))
        return NULL;
    e.dwSize = (DWORD)sizeof(PROCESSENTRY32W);
    if (!Process32FirstW(h, &e)) {
        CloseHandle(h);
        return NULL;
    }
    pid = GetCurrentProcessId();
    do {
        if (e.th32ProcessID == pid) {
            DWORD s = MAX_PATH;
            wchar_t b[MAX_PATH];

            p = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, e.th32ParentProcessID);
            if (p != NULL) {
                if (QueryFullProcessImageNameW(p, 0, b, &s))
                    n = xwcsdup(b);
                CloseHandle(p);
            }
            break;
        }
    } while (Process32NextW(h, &e));
    CloseHandle(h);
    return n;
}

static wchar_t *getposixroot(const wchar_t *rp)
{
    wchar_t *d;
    wchar_t *r = NULL;

    if (rp != NULL) {
        /**
         * Trust user provided root
         */
        return cleanpath(xwcsdup(rp));
    }
    else {
        const wchar_t **e = posixrenv;

        while (*e != NULL) {
            r = xgetenv(*e);
            if (r != NULL) {
                /**
                 * Trust user provided root environment variable
                 */
                return cleanpath(r);
            }
            e++;
        }
        if (r == NULL) {
            r = getparentname();
            if (r != NULL) {
                d = wcsrchr(r, L'\\');
                if (d != NULL)
                    *d = L':';
                d = wcsstr(r, L"\\usr\\bin:");
                if (d == NULL)
                    d = wcsstr(r, L"\\bin:");
                if (d != NULL) {
                    *d = WNUL;
                    return r;
                }
                else {
                    xfree(r);
                    r = NULL;
                }
            }
        }
    }
    if (r != NULL) {
        /* Ensure that path exists */
        d = cleanpath(r);
        r = getrealpathname(d, 1);
        xfree(d);
    }
    return r;
}

static BOOL WINAPI consolehandler(DWORD ctrl)
{
    return TRUE;
}

static int posixmain(int argc, wchar_t **wargv, int envc, wchar_t **wenvp)
{
    int   i, m;
    DWORD rc = 0;
    wchar_t *cmdexe;
    wchar_t *cmdblk;
    wchar_t *envblk;
    PROCESS_INFORMATION cp;
    STARTUPINFOW si;

    for (i = xrunexec; i < argc; i++) {
        wchar_t *v;
        wchar_t *a = wargv[i];

        v = cmdoptionval(a);
        if (v != NULL) {
            wchar_t *q = NULL;
            if (*v == L'"') {
                q = wcsrchr(v + 1, L'"');
                if (q != NULL) {
                    v++;
                    *q = WNUL;
                }
            }
            m = isanypath(v);
            if (m != 0) {
                wchar_t *p = towinpaths(v, m);

                if (q != NULL)
                    p = xwcsappend(p, L'"', NULL);

                v[0] = WNUL;
                wargv[i] = xwcsconcat(a, p);
                xfree(a);
                xfree(p);

            }
        }
        else {
            wargv[i] = pathtowin(a);
        }
        if (xdumparg) {
            if (i > 0)
                fputwc(L'\n', stdout);
            fputws(wargv[i], stdout);
        }
    }
    if (xdumparg)
        return 0;
    for (i = 0; i < (envc - 5); i++) {
        wchar_t *e = wenvp[i];
        const wchar_t **s = xrawenvs;

        while (*s != NULL) {
            if (xwcsisenvvar(e, *s, 0)) {
                /**
                 * Do not convert specified environment variables
                 */
                e = NULL;
                break;
            }
            s++;
        }
        if (e != NULL) {
            int  q = 0;
            wchar_t *v;

            while (e[q] == L'=') {
                /* Skip leading '=' */
                q++;
            }
            v = wcschr(e + q, L'=');
            if (v != NULL) {
                wchar_t *c = NULL;
                v++;
                if (*v == L'/') {
                    c = wcschr(v, L':');
                    if (c != NULL) {
                        *c = WNUL;
                    }
                }
                m = isanypath(v);
                if (c != NULL)
                    *c = L':';
                if (m != 0) {
                    wchar_t *p = towinpaths(v, m);

                    v[0] = WNUL;
                    wenvp[i] = xwcsconcat(e, p);
                    xfree(e);
                    xfree(p);
                }
            }
        }
    }
    qsort((void *)wenvp, envc, sizeof(wchar_t *), envsort);

    memset(&cp, 0, sizeof(PROCESS_INFORMATION));
    memset(&si, 0, sizeof(STARTUPINFOW));

    si.cb = (DWORD)sizeof(STARTUPINFOW);

    cmdexe = xwcsdup(wargv[0]);
    for (i = 0; i < argc; i++) {
        /**
         * Quote arguments
         */
        wargv[i] = xquotearg(wargv[i]);
    }
    cmdblk = xarrblk(argc, wargv, L' ');
    envblk = xarrblk(envc, wenvp, WNUL);
    waafree(wargv);
    waafree(wenvp);
    if (!CreateProcessW(cmdexe, cmdblk,
                        NULL, NULL, TRUE,
                        CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT,
                        envblk, NULL,
                       &si, &cp)) {
        rc = GetLastError();
        if (xshowerr)
            fprintf(stderr, "Failed to execute: '%S'\n",cmdblk);
    }
    else {
        SetConsoleCtrlHandler(consolehandler, TRUE);
        ResumeThread(cp.hThread);
        CloseHandle(cp.hThread);
        /**
         * Wait for child to finish
         */
        WaitForSingleObject(cp.hProcess, INFINITE);
        SetConsoleCtrlHandler(consolehandler, FALSE);

        GetExitCodeProcess(cp.hProcess, &rc);
        CloseHandle(cp.hProcess);
    }

    return rc;
}

int wmain(int argc, const wchar_t **wargv, const wchar_t **wenv)
{
    int i;
    wchar_t **dupwargv = NULL;
    wchar_t **dupwenvp = NULL;
    const wchar_t *crp = NULL;
    wchar_t *ppe       = NULL;
    wchar_t *cwd       = NULL;
    wchar_t *cpp       = NULL;
    wchar_t *wtd;
    wchar_t *ptd;
    wchar_t *npe;
    wchar_t *exe;
    wchar_t *prg;

    int dupenvc = 0;
    int dupargc = 0;
    int envc    = 0;
    int opt     = 0;

    if (wenv == NULL) {
        fputs("\nMissing environment\n", stderr);
        return EBADF;
    }

    while ((opt = xwgetopt(argc, wargv, L"fKn:pqr:vVh?w:")) != EOF) {
        switch (opt) {
            case L'f':
                xforcewp = 1;
            break;
            case L'K':
                xrmendps = 0;
            break;
            case L'n':
                ppe = xwcsappend(ppe, L',', xwoptarg);
            break;
            case L'p':
                xrunexec = 0;
                xdumparg = 1;
            break;
            case L'q':
                xshowerr = 0;
            break;
            case L'r':
                crp = xwoptarg;
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
            case L'w':
                xfree(cwd);
                cwd = xwcsdup(xwoptarg);
            break;
            case EINVAL:
                if (xshowerr)
                    fprintf(stderr, "Error: Invalid command line option: '%C'\n", xwoption);
                return usage(opt);
            break;
            case ENOENT:
                if (xshowerr)
                    fprintf(stderr, "Error: Missing argument for option: '%C'\n", xwoption);
                return usage(opt);
            break;
            default:
                /* Should never happen */
                if (xshowerr)
                    fprintf(stderr, "Error: Unknown return from xwgetopt: %d\n", opt);
                return opt;
            break;

        }
    }
    argc    -= xwoptind;
    wargv   += xwoptind;
    dupwargv = waalloc(argc + 6);
    if (xrunexec) {
        if (argc > 0) {
            dupwargv[0] = xwcsdup(wargv[0]);
            argc--;
            wargv++;
        }
        else {
            if (xshowerr)
                fputs("Missing PROGRAM argument\n", stderr);
            return usage(1);
        }
    }
    posixroot = getposixroot(crp);
    if (posixroot == NULL) {
        if (xshowerr)
            fputs("Cannot find valid POSIX_ROOT\n", stderr);
        return ENOENT;
    }
    /**
     * Check if there is POSIX_PATH variable
     * and use it instead PATH
     */
    cpp = xgetenv(L"POSIX_PATH");
    if (cpp) {
        wchar_t *p = cpp;
        cpp = towinpaths(p, 0);
        xfree(p);
    }
    else {
        wchar_t *p;
        p = xgetenv(L"PATH");
        cpp = towinpaths(p, 0);
        xfree(p);
    }
    if (cpp == NULL) {
        if (xshowerr)
            fputs("Missing PATH environment variable\n", stderr);
        return ENOENT;
    }
    if (xdumparg) {
        for (i = 0; i < argc; i++) {
            if (IS_VALID_WCS(wargv[i]))
                dupwargv[dupargc++] = xwcsdup(wargv[i]);
        }
        return posixmain(dupargc, dupwargv, 0, NULL);
    }
    if (cwd != NULL) {
        if (isrelativepath(cwd))
            cwd = cleanpath(cwd);
        else
            cwd = pathtowin(cwd);
        if (!SetCurrentDirectoryW(cwd)) {
            if (xshowerr)
                fprintf(stderr, "Invalid working directory: '%S'\n", cwd);
            return ENOENT;
        }
        xfree(cwd);
    }
    cwd = xgetcwd();
    if (cwd == NULL) {
        if (xshowerr)
            fputs("Cannot get current working directory\n", stderr);
        return ENOENT;
    }
    ptd = xgetenv(L"TMP");
    if (ptd != NULL)
        ptd = pathtowin(ptd);
    wtd = xgetenv(L"TEMP");
    if (wtd != NULL)
        wtd = pathtowin(wtd);
    else
        wtd = xwcsdup(ptd);
    if (ptd == NULL)
        ptd = xwcsdup(wtd);
    if (ptd == NULL) {
        if (xshowerr)
            fputs("Both TEMP and TMP environment variables are missing\n", stderr);
        return ENOENT;
    }
    npe = xwcsconcat(specialenv, ppe);
    xrawenvs = xsplitstr(npe, L',');
    xfree(npe);
    xfree(ppe);

    while (wenv[envc] != NULL) {
        ++envc;
    }
    dupwenvp = waalloc(envc + 6);
    for (i = 0; i < envc; i++) {
        const wchar_t **e = removeenv;
        const wchar_t  *p = wenv[i];

        while (*e != NULL) {
            if (xwcsisenvvar(p, *e, 1)) {
                /**
                 * Skip private environment variable
                 */
                p = NULL;
                break;
            }
            e++;
        }
        if (p != NULL)
            dupwenvp[dupenvc++] = xwcsdup(p);
    }

    prg = pathtowin(dupwargv[0]);
    exe = getrealpathname(prg, 0);
    if (exe == NULL) {
        exe = xsearchexe(cwd, prg);
        if (exe == NULL) {
            /**
             * PROGRAM was not found.
             * Search inside PATH
             */
            exe = xsearchexe(cpp, prg);
        }
    }
    if (exe == NULL) {
        if (xshowerr)
            fprintf(stderr, "Cannot find PROGRAM '%S'\n", prg);
        return ENOENT;
    }
    xfree(prg);
    if (xisbatchfile(exe)) {
        /**
         * Execute batch file using cmd.exe
         */
        dupwargv[dupargc++] = xgetenv(L"COMSPEC");
        dupwargv[dupargc++] = xwcsdup(L"/D");
        dupwargv[dupargc++] = xwcsdup(L"/E:ON");
        dupwargv[dupargc++] = xwcsdup(L"/V:OFF");
        dupwargv[dupargc++] = xwcsdup(L"/C");
    }
    dupwargv[dupargc++] = exe;
    xrunexec = dupargc;

    for (i = 0; i < argc; i++) {
        if (IS_VALID_WCS(wargv[i]))
            dupwargv[dupargc++] = xwcsdup(wargv[i]);
    }

    /**
     * Add back environment variables
     */
    dupwenvp[dupenvc++] = xwcsconcat(L"PATH=", cpp);
    dupwenvp[dupenvc++] = xwcsconcat(L"POSIX_ROOT=", posixroot);
    dupwenvp[dupenvc++] = xwcsconcat(L"PWD=",  cwd);
    dupwenvp[dupenvc++] = xwcsconcat(L"TEMP=", wtd);
    dupwenvp[dupenvc++] = xwcsconcat(L"TMP=",  ptd);
    xfree(cpp);
    xfree(cwd);
    xfree(ptd);
    xfree(wtd);
    return posixmain(dupargc, dupwargv, dupenvc, dupwenvp);
}
