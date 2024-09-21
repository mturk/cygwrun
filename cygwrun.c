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
#include <signal.h>
#include "cygwrun.h"

#define WNUL                    L'\0'
#define BBUFSIZ                  512

#define CYGWRUN_ERRMAX           125
#define CYGWRUN_FAILED           126
#define CYGWRUN_NOEXEC           127
#define CYGWRUN_SIGBASE          128

#define CYGWRUN_KILL_DEPTH         4
#define CYGWRUN_KILL_TIMEOUT     500
#define CYGWRUN_CRTL_C_WAIT     1000

#define CYGWRUN_SIGINT          (CYGWRUN_SIGBASE + SIGINT)
#define CYGWRUN_SIGTERM         (CYGWRUN_SIGBASE + SIGTERM)

#define IS_PSW(_c)              (((_c) == L'/') || ((_c)  == L'\\'))
#define IS_EMPTY_WCS(_s)        (((_s) == NULL) || (*(_s) == WNUL))
#define IS_VALID_WCS(_s)        (((_s) != NULL) && (*(_s) != WNUL))

/**
 * Align to 16 bytes
 */
#define CYGWRUN_ALIGN(_S)       (((_S) + 0x0000000F) & 0xFFFFFFF0)

static DWORD    xtimeout    = INFINITE;
static int      xshowerr    = 1;
static int      xrmendps    = 1;
static int      xenvcount   = 0;
#if PROJECT_ISDEV_VERSION
static int      xnalloc     = 0;
static int      xnmfree     = 0;
#endif
static wchar_t *posixpath   = NULL;
static wchar_t *posixroot   = NULL;
static wchar_t *posixwork   = NULL;

static wchar_t **xenvvars   = NULL;
static wchar_t **xenvvals   = NULL;
static wchar_t **xrawenvs   = NULL;
static wchar_t **xdelenvs   = NULL;

static wchar_t  zerostr[8]  = { 0 };

static const wchar_t *pathmatches[] = {
    L"/cygdrive/?/+*",
    L"/?/+*",
    L"/bin/*",
    L"/dev/*",
    L"/etc/*",
    L"/home/*",
    L"/lib/*",
    L"/mnt/*",
    L"/opt/*",
    L"/run/*",
    L"/sbin/*",
    L"/share/*",
    L"/sys/*",
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
    L"/mnt",
    L"/opt",
    L"/run",
    L"/sbin",
    L"/share",
    L"/sys",
    L"/tmp",
    L"/usr",
    L"/var",
    NULL
};

static const wchar_t *sskipenv =
    L"OS,PATH,PATHEXT,PRINTER,PROCESSOR_*,PROMPT,PS1,PWD,SHLVL,TERM,TZ";

static const wchar_t *ucenvvars[] = {
    L"COMMONPROGRAMFILES",
    L"COMSPEC",
    L"PATH",
    L"PLATFORM",
    L"PROGRAMDATA",
    L"PROGRAMFILES",
    L"SYSTEMDRIVE",
    L"SYSTEMROOT",
    L"TEMP",
    L"TMP",
    L"WINDIR",
    NULL
};

static __inline int xisalpha(int c)
{
    return ((c >= 0x41 && c <= 0x5A) || (c >= 0x61 && c <= 0x7A));
}

static __inline int xisalnum(int c)
{
    return ((c >= 0x41 && c <= 0x5A) || (c >= 0x61 && c <= 0x7A) ||
            (c >= 0x30 && c <= 0x39));
}

static __inline int xisblank(int c)
{
    return (c == 0x20 || c == 0x09);
}

static __inline int xisnonchar(int c)
{
    return (c > 0 && c <= 0x20);
}

static __inline int xisvarchar(int c)
{
    return (xisalnum(c) || c == 0x2D || c == 0x5F);
}

static __inline int xiswcschar(const wchar_t *s, wchar_t c)
{
    if (s && (s[0] == c) && (s[1] == WNUL))
        return 1;
    else
        return 0;
}

static __inline int xicasecmp(int i, int a, int b)
{
    if (i)
        return (towupper(a) == towupper(b));
    else
        return (a == b);
}

static __inline wchar_t *xskipblanks(const wchar_t *str)
{
    wchar_t *s = (wchar_t *)str;
    if (s && *s) {
        while (xisblank(*s))
            s++;
    }
    return s;
}

static __inline size_t xwcslen(const wchar_t *s)
{
    if (IS_EMPTY_WCS(s))
        return 0;
    else
        return wcslen(s);
}

static wchar_t *xwmalloc(size_t size)
{
    size_t   s;
    wchar_t *p;

    s = CYGWRUN_ALIGN((size + 2) * sizeof(wchar_t));
    p = (wchar_t *)malloc(s);
    if (p == NULL) {
        _wperror(L"xwmalloc");
        _exit(ENOMEM);
    }
#if PROJECT_ISDEV_VERSION
    xnalloc++;
#endif
    p[size++] = WNUL;
    p[size]   = WNUL;
    return p;
}

static void *xcalloc(size_t number, size_t size)
{
    size_t  s;
    void   *p;

    s = CYGWRUN_ALIGN((number + 2) * size);
    p = calloc(1, s);
    if (p == NULL) {
        _wperror(L"xcalloc");
        _exit(ENOMEM);
    }
#if PROJECT_ISDEV_VERSION
    xnalloc++;
#endif
    return p;
}

static void xfree(void *m)
{
    if (m != NULL && m != zerostr) {
        free(m);
#if PROJECT_ISDEV_VERSION
        xnmfree++;
#endif
    }
}

static wchar_t **xwaalloc(size_t size)
{
    return (wchar_t **)xcalloc(size, sizeof(wchar_t *));
}

static void xwaafree(wchar_t **array)
{
    wchar_t **ptr = array;

    if (array == NULL)
        return;
    while (*ptr != NULL)
        xfree(*(ptr++));
    xfree(array);
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

static wchar_t *xwgetenv(const wchar_t *s)
{
    DWORD    n;
    wchar_t  e[BBUFSIZ];
    wchar_t *d = NULL;

    if (IS_EMPTY_WCS(s))
        return NULL;
    n =  GetEnvironmentVariableW(s, e, BBUFSIZ);
    if (n != 0) {
        d = xwmalloc(n);
        if (n > BBUFSIZ)
            GetEnvironmentVariableW(s, d, n);
        else
            wmemcpy(d, e, n);
    }
    return d;
}

static int xngetenv(const wchar_t *s)
{
    DWORD    n;
    wchar_t  e[BBUFSIZ];

    if (IS_EMPTY_WCS(s))
        return -1;
    n =  GetEnvironmentVariableW(s, e, BBUFSIZ);
    if ((n == 0) || (n >= BBUFSIZ))
        return -1;

    return (int)wcstol(e, NULL, 10);
}

static wchar_t *xgetpwd(void)
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

static wchar_t *xwcsconcat(const wchar_t *s1, const wchar_t *s2, wchar_t qc)
{
    wchar_t *rp;
    wchar_t *rs;
    size_t   l1;
    size_t   l2;
    size_t   sz;

    l1 = xwcslen(s1);
    l2 = xwcslen(s2);

    sz = l1 + l2;
    if (sz == 0)
        return NULL;
    if (qc)
       sz += 2;
    rs = xwmalloc(sz);
    rp = rs;
    if (qc && (l2 == 0))
        *(rp++) = qc;
    if (l1 > 0) {
        wmemcpy(rp, s1, l1);
        rp += l1;
    }
    if (l2 > 0) {
        if (qc)
            *(rp++) = qc;
        wmemcpy(rp, s2, l2);
        rp += l2;
    }
    if (qc) {
        *(rp++) = qc;
        *(rp++) = WNUL;
    }
    return rs;
}

static wchar_t *xwcsappend(wchar_t *s, const wchar_t *a, wchar_t sc)
{
    wchar_t *p;
    wchar_t *e;
    size_t   n = xwcslen(s);
    size_t   z = xwcslen(a);

    p = xwmalloc(n + z + 1);
    e = p;

    if (n > 0) {
        wmemcpy(e, s, n);
        e += n;
    }
    if (z > 0) {
        if (n && sc)
            *(e++) = sc;
        wmemcpy(e, a, z);
        e += z;
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
        if (xisnonchar(s[i]))
            s[i] = WNUL;
        else
            break;
    }
    if (i > 0) {
        while (xisnonchar(*s))
            s++;
    }
    return s;
}

static int xwcsbegins(const wchar_t *src, const wchar_t *str)
{
    int sa;

    while (*src) {
        if (*str == WNUL)
            return 1;
        sa = towupper(*src++);
        if (sa != *str++)
            return 0;
    }
    return (*str == WNUL);
}

static int xwcsends(const wchar_t *src, const wchar_t *str, size_t sb)
{
    size_t sa;

    sa = xwcslen(src);
    if (sb == 0)
        sb = xwcslen(str);
    if (sa == 0 || sb == 0 || sb > sa)
        return 0;
    src = src + sa - sb;
    while (*src) {
        if (towlower(*src++) != *str++)
            return 0;
    }
    return 1;
}

static int xwcsequals(const wchar_t *src, const wchar_t *str)
{
    int sa;

    while ((sa = towupper(*src++)) == *str++) {
        if (sa == 0)
            return 1;
    }
    return 0;
}

static int xwcsicmp(const wchar_t *s1, const wchar_t *s2)
{
    int c1;
    int c2;

    while (*s1) {
        c1 = towupper(*s1);
        c2 = towupper(*s2);
        if (c1 != c2)
            return (c1 - c2);
        if (c1 == 0)
            break;
        s1++;
        s2++;
    }
    return 0;
}

/**
 * Match = 0, NoMatch = 1, Abort = -1
 * Based loosely on sections of wildmat.c by Rich Salz
 */
static int xwcsmatch(int ic, const wchar_t *wstr, const wchar_t *wexp)
{
    int match;
    int mflag;

    for ( ; *wexp != WNUL; wstr++, wexp++) {
        if (*wstr == WNUL && *wexp != L'*')
            return -1;
        switch (*wexp) {
            case L'*':
                wexp++;
                while (*wexp == L'*')
                    wexp++;
                if (*wexp == WNUL)
                    return 0;
                while (*wstr != WNUL) {
                    int rv = xwcsmatch(ic, wstr++, wexp);
                    if (rv != 1)
                        return rv;
                }
                return -1;
            break;
            case L'?':
                if (!xisalpha(*wstr))
                    return -1;
            break;
            case L'+':
                if (xisnonchar(*wstr))
                    return -1;
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
                    if (xicasecmp(ic, *wexp, *wstr))
                        match = 1;
                    wexp++;
                }
                if (match == mflag)
                    return -1;
            break;
            default:
                if (!xicasecmp(ic, *wexp, *wstr))
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

static int xneedsquote(const wchar_t *str)
{
    const wchar_t *s;

    for (s = str; *s; s++) {
        if (xisblank(*s) || (*s == 0x22))
            return 1;
    }
    return 0;
}

static wchar_t *xquoteprg(wchar_t *s)
{
    int      i;
    wchar_t *d;
    size_t   n;

    if (IS_EMPTY_WCS(s))
        return s;
    if (xneedsquote(s) == 0)
        return s;
    n = xwcslen(s);
    d = xwmalloc(n + 2);
    d[0] = L'"';
    for (i = 0; i < (int)n; i++)
        d[i + 1] = s[i];
    xfree(s);
    d[i + 1] = L'"';
    d[i + 2] = WNUL;

    return d;
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

    /* Perform quoting only if necessary. */
    if (IS_EMPTY_WCS(s))
        return s;
    if (xneedsquote(s) == 0)
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
    if (xisalpha(s[0])) {
        if (s[1] == L':') {
            if (s[2] == WNUL)
                i += 2;
            else if (IS_PSW(s[2]))
                i += 3;
            else
                i  = 0;
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
            if (xwcsmatch(0, str, pathmatches[i]) == 0)
                return i + 100;
            i++;
        }
    }
    else {
        while (pathfixed[i] != NULL) {
            if (wcscmp(str, pathfixed[i]) == 0)
                return i + 200;
            i++;
        }
    }
    return 0;
}

static int isanypath(int m, wchar_t *s)
{
    int      r;
    wchar_t *c = NULL;

    if (IS_EMPTY_WCS(s))
        return 0;
    if (m && *s == L'/') {
        c = wcschr(s, L':');
        if (c != NULL)
            *c = WNUL;
    }
    switch (*s) {
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
    if (c != NULL)
        *c = L':';
    return r;
}

static int xmszcount(const wchar_t *src)
{
    int c;
    const wchar_t *s = src;

    if (IS_EMPTY_WCS(src))
        return 0;
    for (c = 0; *s; s++, c++) {
        while (*s)
            s++;
    }
    return c;
}

static wchar_t *xarrblk(int cnt, const wchar_t **arr, wchar_t sep)
{
    int      i;
    size_t   n;
    size_t   len = 0;
    size_t  *sz;
    wchar_t *ep;
    wchar_t *bp;

    sz = (size_t *)xcalloc(cnt, sizeof(size_t));
    for (i = 0; i < cnt; i++) {
        n = xwcslen(arr[i]);
        sz[i] = n++;
        len  += n;
    }

    bp = xwmalloc(len + 2);
    ep = bp;
    for (i = 0; i < cnt; i++) {
        if (i > 0)
            *(ep++) = sep;
        wmemcpy(ep, arr[i], sz[i]);
        ep += sz[i];
    }
    xfree(sz);
    ep[0] = WNUL;
    ep[1] = WNUL;

    return bp;
}

static int xenvsort(const void *a1, const void *a2)
{
    const wchar_t *s1 = *((wchar_t **)a1);
    const wchar_t *s2 = *((wchar_t **)a2);

    return xwcsicmp(s1, s2);
}

static int xinitenv(void)
{
    int       ec;
    int       ne;
    wchar_t  *es;
    wchar_t  *ep;
    wchar_t  *en;
    wchar_t  *ev;

    es = GetEnvironmentStringsW();
    if (es == NULL)
        return ENOMEM;
    ec = xmszcount(es);
    if (ec == 0) {
        FreeEnvironmentStringsW(es);
        return ENOENT;
    }
    xenvvars  = xwaalloc(ec + 1);
    xenvvals  = xwaalloc(ec + 1);
    for (ne = 0, ep = es; *ep; ep++) {
        if (*ep != L'=') {
            en = ep;
            ev = wcschr(en, L'=');
            if (ev) {
                int i;
                *ev++ = WNUL;
                if (xiswcschar(en, L'_') || xwcsbegins(en, L"CYGWRUN_"))
                    en = NULL;
                if (en && xdelenvs) {
                    for (i = 0; xdelenvs[i]; i++) {
                        if (xwcsmatch(1, en, xdelenvs[i]) == 0) {
                            en = NULL;
                            break;
                        }
                    }
                }
                if (en) {
                    for (i = 0; ucenvvars[i]; i++) {
                        if (xwcsequals(en, ucenvvars[i])) {
                            xenvvars[ne] = xwcsdup(ucenvvars[i]);
                            break;
                        }
                    }
                    if (xenvvars[ne] == NULL)
                        xenvvars[ne] = xwcsdup(en);
                    xenvvals[ne] = xwcsdup(ev);
                    ne++;
                }
                ep = ev;
            }
            else {
                FreeEnvironmentStringsW(es);
                return EBADF;
            }
        }
        while (*ep)
            ep++;
    }
    xenvcount = ne;
    xenvvars[xenvcount] = NULL;
    xenvvals[xenvcount] = NULL;
    FreeEnvironmentStringsW(es);
    return 0;
}

static wchar_t *xenvblock(void)
{
    int      c;
    size_t   n;
    size_t   v;
    size_t   z;
    wchar_t  *bp;
    wchar_t  *eb;
    wchar_t **ea;
    const wchar_t **ep;
    const wchar_t **ev;

    ep = xenvvars;
    ev = xenvvals;
    z  = 0;
    c  = 0;
    while (*ep) {
        n = xwcslen(*ep);
        if (n) {
            v = xwcslen(*ev);
            z = z + v + n + 2;
            c++;
        }
        ep++;
        ev++;
    }
    if (c == 0)
        return NULL;
    ea = xwaalloc(c + 1);
    eb = xwmalloc(z + 1);
    ep = xenvvars;
    ev = xenvvals;
    z  = 0;
    c  = 0;
    while (*ep) {
        n = xwcslen(*ep);
        v = xwcslen(*ev);
        if (n && v) {
            ea[c] = eb + z;
            wmemcpy(eb + z, *ep, ++n);
            z += n;
            wmemcpy(eb + z, *ev, ++v);
            z += v;
            c++;
        }
        ep++;
        ev++;
    }
    eb[z++] = WNUL;
    eb[z++] = WNUL;
    qsort((void *)ea, c, sizeof(wchar_t *), xenvsort);
    bp = eb;
    for (bp = eb; *bp; bp++) {
        while (*bp)
            bp++;
        *(bp++) = L'=';
        while (*bp)
            bp++;
    }
    bp = xarrblk(c, ea, WNUL);
    xfree(eb);
    xfree(ea);

    return bp;
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

static wchar_t *cleanpath(wchar_t *s)
{
    int n = 0;
    int c;
    int i;


    i = (int)xwcslen(s);
    if (i == 0)
        return s;
    if (i > 6) {
        if (IS_PSW(s[0]) && IS_PSW(s[1]) && (s[2] == L'?') &&
            IS_PSW(s[3]) && (s[5] == L':')) {
            s[0] = L'\\';
            s[1] = L'\\';
            s[3] = L'\\';
            s[4] = towupper(s[4]);
            s[6] = L'\\';
            n    = 7;
        }
    }
    if ((n == 0) && (i > 2)) {
        if (xisalpha(s[0]) && (s[1] == L':') && IS_PSW(s[2])) {
            s[0] = towupper(s[0]);
            s[2] = L'\\';
            n    = 3;
        }
    }
    for (c = n; n < i; n++) {
        if (c > 0) {
            if (IS_PSW(s[c - 1]) && (s[n] == L'.') && IS_PSW(s[n + 1])) {
                n++;
                while (IS_PSW(s[n]))
                    n++;
            }
        }
        if (IS_PSW(s[n]))  {
            if (n > 0) {
                while (IS_PSW(s[n + 1]))
                    n++;
            }
            s[n] = L'\\';
        }
        s[c++] = s[n];
    }
    s[c--] = WNUL;
    while (c > 0) {
        if ((s[c] == L';') || xisnonchar(s[c]))
            s[c--] = WNUL;
        else
            break;
    }
    if (xrmendps) {
        while (c > 1) {
            if (((s[c] == L'\\') && (s[c - 1] != L'.')) || (s[c] == L';'))
                s[c--] = WNUL;
            else
                break;
        }
    }
    return s;
}

static wchar_t **xsplitstr(const wchar_t *s, wchar_t sc)
{
    int      c;
    int      x = 0;
    wchar_t  *cx = NULL;
    wchar_t  *ws;
    wchar_t  *es;
    wchar_t **sa;

    c =  xwcsntok(s, sc);
    if (c == 0)
        return NULL;
    ws = xwcsdup(s);
    sa = xwaalloc(c);
    es = xwcsctok(ws, sc, &cx);
    while (es != NULL) {
        es = xwcstrim(es);
        if (*es != WNUL)
            sa[x++] = xwcsdup(es);
        es = xwcsctok(NULL, sc, &cx);
    }
    xfree(ws);
    sa[x] = NULL;
    if (x == 0) {
        xwaafree(sa);
        sa = NULL;
    }
    return sa;
}

static wchar_t **splitpath(const wchar_t *s, wchar_t ps)
{
    int      c;
    int      x = 0;
    wchar_t  *ws;
    wchar_t **sa;
    wchar_t  *cx = NULL;
    wchar_t  *es;

    c =  xwcsntok(s, ps);
    if (c == 0)
        return NULL;
    ws = xwcsdup(s);
    sa = xwaalloc(c);

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
    int  i;
    int  x;
    size_t s[64];
    size_t n;
    size_t len = 0;
    wchar_t *r;
    wchar_t *p;

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
    wchar_t *rp = NULL;
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
        rp = xwcsconcat(windrive, pp + 12, 0);
        cleanpath(rp + 3);
    }
    else if (m == 101) {
        /**
         * /x/... msys2 absolute path
         */
        windrive[0] = towupper(pp[1]);
        rp = xwcsconcat(windrive, pp + 3, 0);
        cleanpath(rp + 3);
    }
    else if (m == 300) {
        return cleanpath(pp);
    }
    else if (m == 301) {
        rp = xwcsdup(posixroot);
    }
    else {
        cleanpath(pp);
        rp = xwcsconcat(posixroot, pp, 0);
    }
    xfree(pp);
    return rp;
}

static wchar_t *pathtowin(wchar_t *pp)
{
    int m;

    m = isanypath(0, pp);
    if (m == 0)
        return pp;
    if (m < 100)
        return cleanpath(pp);
    else
        return posixtowin(pp, m);
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
                m = isposixpath(pa[i]);
                if (m == 0)
                    cleanpath(pa[i]);
                else
                    pa[i] = posixtowin(pa[i], m);
            }
            wp = mergepath(pa);
            xwaafree(pa);
        }
    }
    else {
        pa = splitpath(ps, L':');

        if (pa != NULL) {
            for (i = 0; pa[i] != NULL; i++) {
                m = isposixpath(pa[i]);
                if (m == 0) {
                    xwaafree(pa);
                    return xwcsdup(ps);
                }
                else {
                    pa[i] = posixtowin(pa[i], m);
                }
            }
            wp = mergepath(pa);
            xwaafree(pa);
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

static BOOL WINAPI consolehandler(DWORD ctrl)
{
    fprintf(stdout, "\nconsolehandler %lu\n", ctrl);
    return TRUE;
}

typedef struct xprocinfo_t
{
    int     n;
    DWORD   i;
    HANDLE  h;
} xprocinfo;

static int getsubproctree(HANDLE sh, xprocinfo *pa, DWORD pid,
                          int pp, int rc, int kd, int sz)
{
    int     i;
    int     n = 0;
    int     p = pp;
    PROCESSENTRY32W e;

    e.dwSize = (DWORD)sizeof(PROCESSENTRY32W);
    if (!Process32FirstW(sh, &e)) {
        return 0;
    }
    do {
        if (p >= sz) {
            break;
        }
        if ((e.th32ParentProcessID == pid) && !xwcsends(e.szExeFile, L"conhost.exe", 11)) {
            pa[p].h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_TERMINATE | SYNCHRONIZE,
                                  FALSE, e.th32ProcessID);
            if (pa[p].h) {
                pa[p].n = 0;
                pa[p].i = e.th32ProcessID;
                p++;
                n++;
            }
        }
    } while (Process32Next(sh, &e));
    if (rc < kd) {
        for (i = 0; i < n; i++) {
            pa[pp + i].n = getsubproctree(sh, pa, pa[pp + i].i, p, rc + 1, kd, sz);
            p += pa[pp + i].n;

        }
    }
    return n;
}

static int getproctree(xprocinfo *pa, DWORD pid, int kd, int sz)
{
    int    n = 0;
    HANDLE sh;

    sh = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (sh == INVALID_HANDLE_VALUE)
        return 0;
    getsubproctree(sh, pa, pid, 0, 1, kd, sz);
    CloseHandle(sh);
    while (n < sz && pa[n].h != NULL)
        n++;
    return n;
}

static void xkillchilds(DWORD pid, int kd, DWORD kw, DWORD ec)
{
    int i;
    int n;
    xprocinfo pa[BBUFSIZ];

    memset(pa, 0, sizeof(pa));
    n = getproctree(pa, pid, kd, BBUFSIZ);
    for (i = n - 1; i >= 0; i--) {
        DWORD s = 0;

        if (kw && pa[i].n)
            s = WaitForSingleObject( pa[i].h, kw);
        if (s || !GetExitCodeProcess(pa[i].h, &s))
            s =  STILL_ACTIVE;
        if (s == STILL_ACTIVE)
            TerminateProcess(pa[i].h, ec);

        CloseHandle(pa[i].h);
    }
}

static int runprogram(int argc, wchar_t **argv)
{
    int      i;
    int      m;
    size_t   n;
    DWORD    rc = 0;

    wchar_t *cmdblk = NULL;
    wchar_t *envblk = NULL;

    PROCESS_INFORMATION cp;
    STARTUPINFOW si;

    for (i = 1; i < argc; i++) {
        wchar_t *v;
        wchar_t *a = argv[i];

        v = cmdoptionval(a);
        if (v != NULL) {
            wchar_t  qc = 0;
            wchar_t *qp = NULL;
            if (v[0] == L'"') {
                n = wcslen(v) - 1;
                if ((n > 1) && (v[n] == v[0])) {
                    qc = v[0];
                    qp = v + n;
                    *(qp)  = WNUL;
                    *(v++) = WNUL;
                }
            }
            m = isanypath(1, v);
            if (m != 0) {
                wchar_t *p = towinpaths(v, m);
                if (qp == NULL)
                    v[0] = WNUL;
                argv[i]  = xwcsconcat(a, p, qc);
                xfree(a);
                xfree(p);
            }
            else {
                if (qp != NULL) {
                    *qp = qc;
                    *(--v) = qc;
                }
            }
        }
        else {
            argv[i] = pathtowin(a);
        }
    }
    for (i = 0; i < xenvcount; i++) {
        int x;
        wchar_t *v = xenvvals[i];

        if (xenvvars[i] == zerostr)
            continue;
        if (xrawenvs) {
            for (x = 0; xrawenvs[x]; x++) {
                if (xwcsmatch(1, xenvvars[i], xrawenvs[x]) == 0) {
                    x = -1;
                    break;
                }
            }
            if (x < 0)
                continue;
        }
        m = isanypath(1, v);
        if (m != 0) {
            xenvvals[i] = towinpaths(v, m);
            xfree(v);
        }
    }
    if (argv[0] == zerostr) {
        for (i = 1; i < argc; i++) {
            wchar_t *a = argv[i];
            if (i > 1)
                fputwc(L'\n', stdout);
            if (*a == L'?') {
                int x;
                for (x = 0; x < xenvcount; x++) {
                    if (xwcsicmp(xenvvars[x], a + 1) == 0) {
                        fputws(xenvvals[x], stdout);
                        break;
                    }
                }
                if (x == xenvcount) {
                    if (xshowerr)
                        fprintf(stderr, "Cannot find environment variable: '%S'\n", a + 1);
                    return ENOENT;
                }
            }
            else {
                fputws(a, stdout);
            }
        }
        return 0;
    }
    argv[0] = xquoteprg(argv[0]);
    for (i = 1; i < argc; i++)
        argv[i] = xquotearg(argv[i]);
    cmdblk = xarrblk(argc, argv, L' ');
    envblk = xenvblock();

    memset(&cp, 0, sizeof(PROCESS_INFORMATION));
    memset(&si, 0, sizeof(STARTUPINFOW));
    si.cb = (DWORD)sizeof(STARTUPINFOW);

    SetConsoleCtrlHandler(NULL, FALSE);
    if (!CreateProcessW(argv[0],
                        cmdblk,
                        NULL, NULL, TRUE,
                        CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT,
                        envblk, posixwork,
                       &si, &cp)) {
        if (xshowerr)
            fprintf(stderr, "The sytem failed to execute: '%S' with error: %lu\n",
                    cmdblk, GetLastError());
        rc = CYGWRUN_FAILED;
    }
    else {
        DWORD ws;
        SetConsoleCtrlHandler(consolehandler, TRUE);
        ResumeThread(cp.hThread);
        CloseHandle(cp.hThread);
        /**
         * Wait for child to finish
         */
        rc = STILL_ACTIVE;
        ws = WaitForSingleObject(cp.hProcess, xtimeout);
        if (ws == WAIT_TIMEOUT) {
            /**
             * Process did not finish within xtimeout
             * Rise CTRL+C signal
             */
            SetConsoleCtrlHandler(NULL, TRUE);
            GenerateConsoleCtrlEvent(0, CTRL_C_EVENT);
            SetConsoleCtrlHandler(NULL, FALSE);
            ws = WaitForSingleObject(cp.hProcess, CYGWRUN_CRTL_C_WAIT);
            if (ws == WAIT_OBJECT_0)
                rc = CYGWRUN_SIGINT;
        }
        if (rc == STILL_ACTIVE && GetExitCodeProcess(cp.hProcess, &rc)) {
            if ((rc != STILL_ACTIVE) && (rc > CYGWRUN_ERRMAX))
                rc = CYGWRUN_ERRMAX;
        }
        if (rc == STILL_ACTIVE) {
            xkillchilds(cp.dwProcessId, CYGWRUN_KILL_DEPTH, CYGWRUN_KILL_TIMEOUT, SIGTERM);
            TerminateProcess(cp.hProcess, SIGTERM);
            rc = CYGWRUN_SIGTERM;
        }
        CloseHandle(cp.hProcess);
        SetConsoleCtrlHandler(consolehandler, FALSE);
    }
#if PROJECT_ISDEV_VERSION
    xfree(envblk);
    xfree(cmdblk);
#endif
    return rc;
}

int wmain(int argc, const wchar_t **argv)
{
    int       i;
    int       rv;
    int       dupargc;
    wchar_t **dupargv;
    wchar_t  *exe;
    wchar_t  *prg;

    wchar_t  *eparam;
    wchar_t  *cwdir = NULL;
    wchar_t  *wendp = NULL;


    /**
     * Make sure child processes are kept quiet.
     */
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX | SEM_NOGPFAULTERRORBOX);
    if (argc == 0)
        return ENOSYS;
    if (argc == 1) {
        fputs(PROJECT_NAME " version " PROJECT_VERSION_ALL, stdout);
        return 0;
    }
    if (xngetenv(L"CYGWRUN_QUIET") == 1)
        xshowerr = 0;
    if (xngetenv(L"CYGWRUN_TRIM_PATHS") == 0)
        xrmendps = 0;
    --argc;
    ++argv;
    if (argc && *(argv[0]) == L'~') {
        posixroot = getrealpathname(argv[0] + 1, 1);
        if (posixroot == NULL) {
            if (xshowerr)
                fprintf(stderr, "\nThe system cannot find the CYGWRUN_ROOT directory: '%S'\n", argv[0] + 1);
            return ENOENT;
        }
        --argc;
        ++argv;
    }
    if (argc && *(argv[0]) == L'@') {
        cwdir = xwcsdup(argv[0] + 1);
        --argc;
        ++argv;
    }
    if (argc && *(argv[0]) == L'^') {
        rv = (int)wcstol(argv[0] + 1, &wendp, 10);
        if ((*wendp != WNUL) || ((rv == 0) && (errno != 0))) {
            if (xshowerr)
                fputs("\nThe timeout parameter is invalid\n", stderr);
            return EINVAL;
        }
        if (rv > 0) {
            if ((rv < 2) || (rv > 2000000)) {
                if (xshowerr)
                    fputs("\nThe timeout parameter is outside valid range\n", stderr);
                return ERANGE;
            }
            xtimeout = rv * 1000;
        }
        --argc;
        ++argv;
    }
    else {
        rv = xngetenv(L"CYGWRUN_TIMEOUT");
        if (rv > 0) {
            if ((rv < 2) || (rv > 2000000)) {
                if (xshowerr)
                    fputs("\nThe CYGWRUN_TIMEOUT is outside valid range\n", stderr);
                return ERANGE;
            }
            xtimeout = rv * 1000;
        }
    }
    eparam = xwgetenv(L"CYGWRUN_SKIP");
    if (argc && *(argv[0]) == L'=') {
        eparam = xwcsappend(eparam, argv[0] + 1, L',');
        --argc;
        ++argv;
    }
    eparam   = xwcsappend(eparam, sskipenv, L',');
    xrawenvs = xsplitstr(eparam, L',');
    xfree(eparam);
    eparam = xwgetenv(L"CYGWRUN_UNSET");
    if (argc && *(argv[0]) == L'-') {
        eparam = xwcsappend(eparam, argv[0] + 1, L',');
        --argc;
        ++argv;
    }
    if (eparam) {
        xdelenvs = xsplitstr(eparam, L',');
        xfree(eparam);
    }
    if (argc < 1) {
        if (xshowerr)
            fputs("\nThe PROGRAM name was not specified\n", stderr);
        return ENOENT;
    }
    if (posixroot == NULL) {
        eparam = xwgetenv(L"CYGWRUN_ROOT");
        if (eparam == NULL) {
            if (xshowerr)
                fputs("\nThe CYGWRUN_ROOT environment variable was not found\n", stderr);
            return ENOENT;
        }
        posixroot = getrealpathname(eparam, 1);
        if (posixroot == NULL) {
            if (xshowerr)
                fprintf(stderr, "\nThe system cannot find the CYGWRUN_ROOT directory: '%S'\n", eparam);
            return CYGWRUN_NOEXEC;
        }
        xfree(eparam);
    }
    if (cwdir) {
        cwdir = pathtowin(cwdir);
        if (cwdir == NULL) {
            if (xshowerr)
                fputs("The working directory path is invalid\n", stderr);
            return EBADF;
        }
        posixwork = getrealpathname(cwdir, 1);
        if (posixwork == NULL) {
            if (xshowerr)
                fprintf(stderr, "\nThe system cannot find the work directory: '%S'\n", cwdir);
            return ENOTDIR;
        }
        xfree(cwdir);
    }
    else {
        posixwork = xgetpwd();
        if (posixwork == NULL) {
            if (xshowerr)
                fputs("The system cannot find the current working directory\n", stderr);
            return ENOENT;
        }
    }
    eparam = xwgetenv(L"CYGWRUN_PATH");
    if (eparam) {
        posixpath = towinpaths(eparam, 0);
        xfree(eparam);
    }
    if (posixpath == NULL) {
        posixpath = xwgetenv(L"PATH");
        posixpath = cleanpath(posixpath);
    }
    if (posixpath == NULL) {
        if (xshowerr)
            fputs("The PATH environment variable was not defined\n", stderr);
        return ENOENT;
    }
    SetEnvironmentVariableW(L"PATH", posixpath);
    SetEnvironmentVariableW(L"PWD",  posixwork);

    rv = xinitenv();
    if (rv) {
        if (xshowerr)
            fputs("The system was unable to init the environment\n", stderr);
        return rv;
    }
    dupargc = 0;
    dupargv = xwaalloc(argc);
    if (xiswcschar(argv[0], L'.')) {
        dupargv[dupargc++] = zerostr;
        if (argc < 2) {
            if (xshowerr)
                fputs("\nThere are no options to evaluate\n", stderr);
            return EINVAL;
        }
    }
    else {
        prg = pathtowin(xwcsdup(argv[0]));
        if (prg == NULL) {
            if (xshowerr)
                fprintf(stderr, "The program path is invalid: '%S'\n", argv[0]);
            return CYGWRUN_NOEXEC;
        }
        exe = getrealpathname(prg, 0);
        if (exe == NULL) {
            exe = xsearchexe(posixwork, prg);
            if (exe == NULL) {
                /**
                 * PROGRAM was not found in the work directory
                 * Search inside PATH
                 */
                exe = xsearchexe(posixpath, prg);
            }
        }
        if (exe == NULL) {
            if (xshowerr)
                fprintf(stderr, "The system cannot find the executable: '%S'\n", prg);
            return CYGWRUN_NOEXEC;
        }
        xfree(prg);
        dupargv[dupargc++] = exe;
    }
    for (i = 1; i < argc; i++)
        dupargv[dupargc++] = xwcsdup(argv[i]);

    rv = runprogram(dupargc, dupargv);
#if PROJECT_ISDEV_VERSION
    xfree(posixpath);
    xfree(posixroot);
    xfree(posixwork);
    xwaafree(xenvvals);
    xwaafree(xenvvars);
    xwaafree(dupargv);
    xwaafree(xrawenvs);
    xwaafree(xdelenvs);
    if (xnalloc != xnmfree)
        fprintf(stderr, "\nAllocated: %d\nFree     : %d\n", xnalloc, xnmfree);
#endif
    return rv;
}
