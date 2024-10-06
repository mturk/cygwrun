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
#include "cygwrun.h"

/**
 * Use Win32 heapapi instead malloc/free
 */
#define CYGWRUN_USE_HEAPAPI         1
#define CYGWRUN_USE_PRIVATE_HEAP    0
/**
 * Call HeapFree/free
 * Set this value to 1 for dev versions
 */
#define CYGWRUN_USE_MEMFREE         0
/**
 * Use -s and -u command options
 */
#define CYGWRUN_HAVE_CMDOPTS        0

#define CYGWRUN_ERRMAX            110
#define CYGWRUN_FAILED            126
#define CYGWRUN_ENOEXEC           127
#define CYGWRUN_SIGBASE           128

#define CYGWRUN_ENOSYS            111   /** Cannot find cygwin root     */
#define CYGWRUN_EINVAL            112
#define CYGWRUN_EPARAM            113
#define CYGWRUN_EALREADY          114
#define CYGWRUN_ENOENT            115
#define CYGWRUN_EEMPTY            116
#define CYGWRUN_EBADPATH          117

#define CYGWRUN_ENOSPC            118
#define CYGWRUN_ENOENV            119
#define CYGWRUN_EBADENV           120
#define CYGWRUN_ENOMEM            121
#define CYGWRUN_ERANGE            122

#define CYGWRUN_MAX_ALLOC      131072   /** Limit single alloc to 128K  */
#define CYGWRUN_PATH_MAX         4096
#define CYGWRUN_KILL_DEPTH          4
#define CYGWRUN_KILL_SIZE         256
#define CYGWRUN_KILL_TIMEOUT      500
#define CYGWRUN_CRTL_C_WAIT      2000
#define CYGWRUN_CRTL_S_WAIT      3000

#define CYGWRUN_SIGINT          (CYGWRUN_SIGBASE +  2)
#define CYGWRUN_SIGTERM         (CYGWRUN_SIGBASE + 15)

#define IS_PSW(_c)              (((_c) == L'/') || ((_c)  == L'\\'))
#define IS_EMPTY_WCS(_s)        (((_s) == NULL) || (*(_s) == 0))
#define IS_EMPTY_STR(_s)        (((_s) == NULL) || (*(_s) == 0))


/**
 * Align to 16 bytes
 */
#define CYGWRUN_ALIGN(_S)       (((_S) + 0x0000000F) & 0xFFFFFFF0)

#if CYGWRUN_USE_HEAPAPI
static HANDLE      memheap      = NULL;
#if CYGWRUN_USE_MEMFREE && CYGWRUN_ISDEV_VERSION
static size_t      xzmfree      = 0;
#endif
#endif

static HANDLE      conevent     = NULL;
static HANDLE      cprocess     = NULL;
static int         xrmendps     = L'\\';
static int         xenvcount    = 0;
#if CYGWRUN_USE_MEMFREE && CYGWRUN_ISDEV_VERSION
static size_t      xzalloc      = 0;
static int         xnalloc      = 0;
static int         xnmfree      = 0;
#endif
static wchar_t    *posixroot    = NULL;
static wchar_t    *posixpath    = NULL;

static wchar_t   **xenvvars     = NULL;
static wchar_t   **xenvvals     = NULL;
static wchar_t   **askipenv     = NULL;
static char      **adelenvv     = NULL;

static char      **systemenvn   = NULL;
static char      **systemenvv   = NULL;
static int         systemenvc   = 0;

static const char *configvals[8]    = { NULL };
static wchar_t     zerowcs[8]       = { 0 };

typedef enum {
    CYGWRUN_PATH     = 0,
    CYGWRUN_SKIP,
    CYGWRUN_UNSET,
    CCYGWIN_PATH,
    CCYGWIN_TEMP,
    CCYGWIN_TMP

} CYGWRUN_CONFIG_VARS;

static const char *configvars[]     = {
    "CYGWRUN_PATH",
    "CYGWRUN_SKIP",
    "CYGWRUN_UNSET",
    "PATH",
    "TEMP",
    "TMP",
    NULL
};

static const wchar_t *rootpaths[]   = {
    L"/bin/",
    L"/dev/",
    L"/etc/",
    L"/home/",
    L"/lib/",
    L"/sbin/",
    L"/tmp/",
    L"/usr/",
    L"/var/",
    NULL
};

static const char *sskipenv =
    "COMPUTERNAME,HOMEDRIVE,HOMEPATH,HOST," \
    "HOSTNAME,LOGONSERVER,PATH,PATHEXT,PROCESSOR_@*,PROMPT,USER,USERNAME";

static const char *unsetvars[] = {
    "ERRORLEVEL",
    "EXECIGNORE",
    "INFOPATH",
    "LANG",
    "OLDPWD",
    "ORIGINAL_PATH",
    "PROFILEREAD",
    "PS1",
    "PWD",
    "SHELL",
    "SHLVL",
    "TERM",
    "TZ",
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

static __inline int xisnonchar(int c)
{
    return (c > 0 && c <= 0x20);
}

static __inline int xisspace(int c)
{
    return ((c >= 0x09 && c <= 0x0D) || (c == 0x20));
}

static __inline int xisvarchar(int c)
{
    return (xisalnum(c) || c == 0x2D || c == 0x5F);
}

static __inline int xtolower(int c)
{
    return (c >= 0x41 && c <= 0x5A) ? c ^ 0x20 : c;
}

static __inline int xtoupper(int c)
{
    return (c >= 0x61 && c <= 0x7A) ? c ^ 0x20 : c;
}

static size_t xstrlen(const char *src)
{
    const char *s = src;

    if (IS_EMPTY_STR(s))
        return 0;
    while (*s)
        s++;
    return (size_t)(s - src);
}

static size_t xwcslen(const wchar_t *src)
{
    const wchar_t *s = src;

    if (IS_EMPTY_WCS(s))
        return 0;
    while (*s)
        s++;
    return (size_t)(s - src);
}

static void *xalloc(size_t size)
{
    size_t s;
    void  *p;

    s = CYGWRUN_ALIGN(size);
    if (s > CYGWRUN_MAX_ALLOC)
        exit(CYGWRUN_ERANGE);
#if CYGWRUN_USE_HEAPAPI
    p = HeapAlloc(memheap, HEAP_ZERO_MEMORY, s);
#else
    p = calloc(1, s);
#endif
    if (p == NULL)
        exit(CYGWRUN_ENOMEM);
#if CYGWRUN_USE_MEMFREE && CYGWRUN_ISDEV_VERSION
    xzalloc += s;
    xnalloc++;
#endif
    return p;
}

static wchar_t *xwalloc(size_t size)
{
    return (wchar_t *)xalloc((size + 2) * sizeof(wchar_t));
}

static char *xmalloc(size_t size)
{
    return (char *)xalloc(size + 2);
}

static void *xcalloc(size_t number, size_t size)
{
    return xalloc((number + 2) * size);
}

static wchar_t **xwaalloc(size_t size)
{
    return (wchar_t **)xcalloc(size, sizeof(wchar_t *));
}

static char **xsaalloc(size_t size)
{
    return (char **)xcalloc(size, sizeof(char *));
}

#if CYGWRUN_USE_MEMFREE
static void xmfree(void *m)
{
    if (m != NULL && m != zerowcs) {
#if CYGWRUN_USE_HEAPAPI
#if CYGWRUN_ISDEV_VERSION
        xzmfree += HeapSize(memheap, 0, m);
#endif
        HeapFree(memheap, 0, m);
#else
        free(m);
#endif
#if CYGWRUN_ISDEV_VERSION
        xnmfree++;
#endif
    }
}

static void xafree(void **array)
{
    void **ptr = array;

    if (array == NULL)
        return;
    while (*ptr != NULL)
        xmfree(*(ptr++));
    xmfree(array);
}
#else
#define xmfree(_m)      (void)0
#define xafree(_m)      (void)0
#endif


static wchar_t *xwcsdup(const wchar_t *s)
{
    wchar_t *p;
    size_t   n;

    n = xwcslen(s);
    if (n == 0)
        return NULL;
    p = xwalloc(n);
    return wmemcpy(p, s, n);
}

static char *xstrdup(const char *s)
{
    char    *p;
    size_t   n;

    n = xstrlen(s);
    if (n == 0)
        return NULL;
    p = xmalloc(n);
    return memcpy(p, s, n);
}

static wchar_t *xwcschr(const wchar_t *src, const wchar_t *exc, wchar_t c)
{
    const wchar_t *e;
    const wchar_t *s = src;
    while (*s) {
        if (exc) {
            for (e = exc; *e; e++) {
                if (*e == c)
                    return NULL;
            }
        }
        if (*s == c)
            return (wchar_t *)s;
        s++;
    }
    return NULL;
}

static char *xstrchr(const char *src, const char *exc, int c)
{
    const char *e;
    const char *s = src;
    while (*s) {
        if (exc) {
            for (e = exc; *e; e++) {
                if (*e == c)
                    return NULL;
            }
        }
        if (*s == c)
            return (char *)s;
        s++;
    }
    return NULL;
}

static size_t xstrchrn(const char *str, char ch)
{
    const char *s = str;
    while (*s) {
        if (*s == ch)
            return (size_t)(s - str);
        s++;
    }
    return 0;
}

static wchar_t *xmbstowcs(const char *mbs)
{
    wchar_t *wcs;
    int      mbl;

    mbl = (int)xstrlen(mbs);
    if (mbl < 1)
        return NULL;
    wcs = xwalloc(mbl);
    if (MultiByteToWideChar(CP_UTF8, 0, mbs, mbl, wcs, mbl + 1) == 0) {
        xmfree(wcs);
        wcs = NULL;
    }
    return wcs;
}

static char *xwcstombs(const wchar_t *wcs)
{
    char    *mbs;
    int      wcl;
    int      mbl;

    wcl = (int)xwcslen(wcs);
    if (wcl < 1)
        return NULL;
    mbl = (wcl * 3);
    mbs = xmalloc(mbl);
    if (WideCharToMultiByte(CP_UTF8, 0, wcs, wcl, mbs, mbl + 1, NULL, NULL) == 0) {
        xmfree(mbs);
        mbs = NULL;
    }
    return mbs;
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
    rs = xwalloc(sz);
    rp = rs;
    if (qc && !l2)
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
        *(rp++) = 0;
    }
    return rs;
}

static wchar_t *xwcsappend(wchar_t *s, const wchar_t *a, wchar_t sc)
{
    wchar_t *p;
    wchar_t *e;
    size_t   n = xwcslen(s);
    size_t   z = xwcslen(a);

    p = xwalloc(n + z + 1);
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
    *e = 0;
    xmfree(s);
    return p;
}

static char *xstrappend(char *s, const char *a, char sc)
{
    char   *p;
    char   *e;
    size_t  n = xstrlen(s);
    size_t  z = xstrlen(a);

    if (z == 0)
        return s;
    p = xmalloc(n + z + 1);
    e = p;

    if (n > 0) {
        memcpy(e, s, n);
        e += n;
    }
    if (z > 0) {
        if (n && sc)
            *(e++) = sc;
        memcpy(e, a, z);
        e += z;
    }
    *e = 0;
    xmfree(s);
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
            s[i] = 0;
        else
            break;
    }
    if (i > 0) {
        while (xisnonchar(*s))
            s++;
    }
    return s;
}

static char *xstrtrim(char *s)
{
    size_t i = xstrlen(s);

    while (i > 0) {
        i--;
        if (xisnonchar(s[i]))
            s[i] = 0;
        else
            break;
    }
    if (i > 0) {
        while (xisnonchar(*s))
            s++;
    }
    return s;
}

char *xstrbegins(int ic, const char *src, const char *str)
{
    const char *pos = src;
    int sa;
    int sb;

    while (*src) {
        if (*str == 0)
            return (char *)pos;
        if (ic) {
            sa = xtolower(*src++);
            sb = xtolower(*str++);
        }
        else {
            sa = *src++;
            sb = *str++;
        }
        if (sa != sb)
            return NULL;
        pos++;
    }
    return *str ? NULL : (char *)pos;
}

char *xstrnbegins(int ic, const char *src, const char *str, size_t len)
{
    const char *pos = src;
    int sa;
    int sb;

    while (*src) {
        if (*str == 0  || len == 0)
            return (char *)pos;
        if (ic) {
            sa = xtolower(*src++);
            sb = xtolower(*str++);
        }
        else {
            sa = *src++;
            sb = *str++;
        }
        if (sa != sb)
            return NULL;
        pos++;
        len--;
    }
    return len ? NULL : (char *)pos;
}

static int xstrnicmp(const char *s1, const char *s2, size_t n)
{
    int sa;
    int sb;

    if (n == 0)
        return 0;
    do {
        sa = xtoupper(*s1);
        sb = xtoupper(*s2);
        if (sa != sb)
            return (sa - sb);
        if (sa == 0)
            break;
        s1++;
        s2++;
    } while (--n != 0);
    return 0;
}

static int xwcsbegins(const wchar_t *src, const wchar_t *str)
{
    while (*src) {
        if (*str == 0)
            return 1;
        if (*src++ != *str++)
            return 0;
    }
    return (*str == 0);
}

static int xwcsequals(const wchar_t *src, const wchar_t *str, wchar_t ech)
{
    while (*src) {
        if (*str == 0)
            return (*src == 0);
        if (*src != *str)
            return 0;
        src++;
        str++;
    }
    return (*str == ech);
}

static int xwcsicmp(const wchar_t *s1, const wchar_t *s2)
{
    int c1;
    int c2;

    while ((c1 = xtoupper(*s1++)) == (c2 = xtoupper(*s2++))) {
        if (c1 == 0)
            return 0;
    }
    return (c1 - c2);
}

static int xstricmp(const char *s1, const char *s2)
{
    int c1;
    int c2;

    while ((c1 = xtoupper(*s1++)) == (c2 = xtoupper(*s2++))) {
        if (c1 == 0)
            return 0;
    }
    return (c1 - c2);
}

/**
 * Match string to expression.
 * Match = 0, NoMatch = 1, Abort = -1
 * Based loosely on sections of wildmat.c by Rich Salz
 *
 * '*' matches any char
 * '@' character must be alphabetic
 * '+' character must be not be control or space
 */
static int xwcsimatch(const wchar_t *wstr, const wchar_t *wexp)
{
    for ( ; *wexp != 0; wstr++, wexp++) {
        if (*wstr == 0 && *wexp != L'*')
            return -1;
        switch (*wexp) {
            case L'*':
                wexp++;
                while (*wexp == L'*')
                    wexp++;
                if (*wexp == 0)
                    return 0;
                while (*wstr != 0) {
                    int m = xwcsimatch(wstr++, wexp);
                    if (m != 1)
                        return m;
                }
                return -1;
            break;
            case L'@':
                if (!xisalpha(*wstr))
                    return -1;
            break;
            case L'+':
                if (xisnonchar(*wstr))
                    return -1;
            break;
            default:
                if (xtoupper(*wexp) != xtoupper(*wstr))
                    return 1;
            break;
        }
    }
    return (*wstr != 0);
}

static int xstrimatch(const char *str, const char *exp)
{
    for ( ; *exp; str++, exp++) {
        if (*str == 0 && *exp != '*')
            return -1;
        switch (*exp) {
            case '*':
                exp++;
                while (*exp == '*')
                    exp++;
                if (*exp == 0)
                    return 0;
                while (*str) {
                    int m = xstrimatch(str++, exp);
                    if (m != 1)
                        return m;
                }
                return -1;
            break;
            case '@':
                if (!xisalpha(*str))
                    return -1;
            break;
            case '+':
                if (xisnonchar(*str))
                    return -1;
            break;
            default:
                if (xtoupper(*exp) != xtoupper(*str))
                    return 1;
            break;
        }
    }
    return (*str != 0);
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
    if (*s == 0)
        return 0;
    while (*s != 0) {
        if (*(s++) == d) {
            while (*s == d) {
                s++;
            }
            if (*s != 0)
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
    if (*s == 0)
        return NULL;
    p = s;

    while (*s != 0) {
        if (*s == d) {
            *s = 0;
            *c = s + 1;
            break;
        }
        s++;
    }
    return p;
}

static int xstrntok(const char *s, char d)
{
    int n = 1;

    while (*s == d) {
        s++;
    }
    if (*s == 0)
        return 0;
    while (*s != 0) {
        if (*(s++) == d) {
            while (*s == d) {
                s++;
            }
            if (*s != 0)
                n++;
        }
    }
    return n;
}

/**
 * This is strtok_s clone using single character as token delimiter
 */
static char *xstrctok(char *s, char d, char **c)
{
    char *p;

    if ((s == NULL) && ((s = *c) == NULL))
        return NULL;

    *c = NULL;
    /**
     * Skip leading delimiter
     */
    while (*s == d) {
        s++;
    }
    if (*s == 0)
        return NULL;
    p = s;

    while (*s != 0) {
        if (*s == d) {
            *s = 0;
            *c = s + 1;
            break;
        }
        s++;
    }
    return p;
}

static int xneedsquote(const wchar_t *s)
{
    if (s && *s) {
        while (*s) {
            if (xisspace(*s) || (*s == 0x22))
                return 1;
            s++;
        }
    }
    return 0;
}

static wchar_t *xwcsquote(wchar_t *s)
{
    size_t   n;
    wchar_t *d;
    wchar_t *e;

    if (!xneedsquote(s))
        return s;
    n = xwcslen(s);
    e = xwalloc(n + 2);
    d = e;
    *(d++) = L'"';
    wmemcpy(d, s, n);
    d += n;
    xmfree(s);
    *(d++) = L'"';
    *(d)   = 0;

    return e;
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
    if (!xneedsquote(s))
        return s;
    for (c = s; ; c++) {
        size_t b = 0;

        while (*c == L'\\') {
            b++;
            c++;
        }

        if (*c == 0) {
            n += b * 2;
            break;
        }
        else if (*c == L'"') {
            n += b * 2 + 1;
            n += 1;
        }
        else {
            n += b;
            n += 1;
        }
    }
    n += 2;
    e = xwalloc(n);
    d = e;

    *(d++) = L'"';
    for (c = s; ; c++) {
        size_t b = 0;

        while (*c == '\\') {
            b++;
            c++;
        }

        if (*c == 0) {
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
    xmfree(s);
    *(d++) = L'"';
    *(d)   = 0;

    return e;
}

static int iswinpath(const wchar_t *s)
{
    int i = 0;

    if (IS_PSW(s[0])) {
        if (IS_PSW(s[1]) && (s[2] != 0)) {
            i  = 2;
            s += 2;
            if ((s[0] == L'?' ) &&
                (IS_PSW(s[1]) ) &&
                (s[2] != 0)) {
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
            if (s[2] == 0)
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

static int ispathlist(const wchar_t *str)
{
    const wchar_t *s = str;
    int   ccolon     = 1;
    int   pathss     = 0;
    int   hcolon     = 0;

    if ((*s == L';') || (*s == L':'))
        return *s;
    /**
     * Check if the first elem is windows path
     */
    if ((*s == L'\\') || iswinpath(s))
        ccolon = 0;
    while (*s) {
        if (*s ==  L';')
            return L';';
        if (*s ==  L'/')
            pathss = 1;
        if (pathss && ccolon && (*s == L':'))
            hcolon = L':';

        s++;
    }
    return hcolon;
}

static int isposixpath(const wchar_t *str)
{
    int i = 0;

    if (str[0] != L'/')
        return isdotpath(str);
    if (str[1] == 0)
        return 301;
    if (str[1] == L'/')
        return iswinpath(str);
    if (xwcschr(str + 1, L":;", L'/')) {
        if (xwcsbegins(str, L"/cygdrive/") &&
            xisalpha(str[10]) && (str[11] == L'/') && !xisnonchar(str[12]))
            return 100;
        while (rootpaths[i] != NULL) {
            if (xwcsbegins(str, rootpaths[i]))
                return i + 101;
            i++;
        }
    }
    else {
        while (rootpaths[i] != NULL) {
            if (xwcsequals(str, rootpaths[i], L'/'))
                return i + 200;
            i++;
        }
    }
    return 0;
}

static int isanypath(int m, wchar_t *s)
{
    int r;
    if (IS_EMPTY_WCS(s) || (*s == L'\''))
        return 0;
    if (m) {
        r = ispathlist(s);
        if (r)
            return r;
    }
    switch (*s) {
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

static wchar_t *warraytomsz(int cnt, const wchar_t **arr, wchar_t sep)
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

    bp = xwalloc(len + 2);
    ep = bp;
    for (i = 0; i < cnt; i++) {
        if (i > 0)
            *(ep++) = sep;
        wmemcpy(ep, arr[i], sz[i]);
        ep += sz[i];
    }
    xmfree(sz);
    ep[0] = 0;
    ep[1] = 0;

    return bp;
}

static int sortenvvars(const void *a1, const void *a2)
{
    const wchar_t *s1 = *((wchar_t **)a1);
    const wchar_t *s2 = *((wchar_t **)a2);

    return xwcsicmp(s1, s2);
}

static wchar_t *getenvblock(void)
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
    eb = xwalloc(z + 1);
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
    eb[z++] = 0;
    eb[z++] = 0;
    qsort((void *)ea, c, sizeof(wchar_t *), sortenvvars);
    bp = eb;
    for (bp = eb; *bp; bp++) {
        while (*bp)
            bp++;
        *(bp++) = L'=';
        while (*bp)
            bp++;
    }
    bp = warraytomsz(c, ea, 0);
    xmfree(eb);
    xmfree(ea);

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
    while (*s != 0) {
        wchar_t c = *(s++);
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

static wchar_t *wcleanpath(wchar_t *s)
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
            s[4] = (wchar_t)xtoupper(s[4]);
            s[6] = L'\\';
            n    = 7;
        }
    }
    if ((n == 0) && (i > 2)) {
        if (xisalpha(s[0]) && (s[1] == L':') && IS_PSW(s[2])) {
            s[0] = (wchar_t)xtoupper(s[0]);
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
    s[c--] = 0;
    while (c > 0) {
        if ((s[c] == L';') || ((s[c] == xrmendps) && (s[c - 1] != L'.')) || xisnonchar(s[c]))
            s[c--] = 0;
        else
            break;
    }
    return s;
}

static wchar_t **wcstoarray(const wchar_t *s, wchar_t sc)
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
        if (*es != 0)
            sa[x++] = xwcsdup(es);
        es = xwcsctok(NULL, sc, &cx);
    }
    xmfree(ws);
    sa[x] = NULL;
    if (x == 0) {
        xafree(sa);
        sa = NULL;
    }
    return sa;
}

static char **strtoarray(const char *s, char sc)
{
    int      c;
    int      x = 0;
    char    *cx = NULL;
    char    *ws;
    char    *es;
    char   **sa;

    if (IS_EMPTY_STR(s))
        return NULL;
    c =  xstrntok(s, sc);
    if (c == 0)
        return NULL;
    ws = xstrdup(s);
    sa = xsaalloc(c);
    es = xstrctok(ws, sc, &cx);
    while (es != NULL) {
        es = xstrtrim(es);
        if (*es != 0)
            sa[x++] = xstrdup(es);
        es = xstrctok(NULL, sc, &cx);
    }
    xmfree(ws);
    sa[x] = NULL;
    if (x == 0) {
        xafree(sa);
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
        if (*es != 0)
            sa[x++] = xwcsdup(es);
        es = xwcsctok(NULL, ps, &cx);
    }
    xmfree(ws);
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
        n = xwcslen(pp[i]);
        if (x < 64)
            s[x] = n;
        len += (n + 1);
    }
    r = p = xwalloc(len + 2);
    for (i = 0, x = 0; pp[i] != NULL; i++, x++) {
        if (x < 64)
            n = s[x];
        else
            n = xwcslen(pp[i]);
        if (n) {
            if (i > 0 && p > r)
                *(p++) = L';';
            wmemcpy(p, pp[i], n);
            p += n;
        }
    }
    *p = 0;
    return r;
}

static wchar_t *posixtowin(wchar_t *pp, int m)
{
    wchar_t *rp = NULL;
    wchar_t  windrive[] = { 0, L':', L'\\', 0};

    if (m == 0)
        m = isposixpath(pp);
    if (m == 0) {
        /**
         * Not a posix path
         */
        return pp;
    }
    else if (m <  100) {
        return wcleanpath(pp);
    }
    else if (m == 100) {
        /**
         * /cygdrive/x/... absolute path
         */
        windrive[0] = (wchar_t)xtoupper(pp[10]);
        rp = xwcsconcat(windrive, pp + 12, 0);
        wcleanpath(rp + 3);
    }
    else if (m == 300) {
        if (ispathlist(pp) == L':')
            return pp;
        else
            return wcleanpath(pp);
    }
    else if (m == 301) {
        rp = xwcsdup(posixroot);
    }
    else {
        pp = wcleanpath(pp);
        if (*pp != L'\\')
            return pp;
        rp = xwcsconcat(posixroot, pp, 0);
    }
    xmfree(pp);
    return rp;
}

static wchar_t *pathtowin(wchar_t *pp)
{
    int m;

    m = isanypath(0, pp);
    if (m)
        return posixtowin(pp, m);
    else
        return pp;
}

static wchar_t *pathstowin(const wchar_t *ps)
{
    int i;
    int m;
    wchar_t   sc = 0;
    wchar_t **pa;
    wchar_t  *wp = NULL;

    sc = (wchar_t)ispathlist(ps);
    if (sc == 0) {
        /* Not a path list */
        wp = xwcsdup(ps);
        m  = isposixpath(wp);
        if (m == 0)
            wp = wcleanpath(wp);
        else
            wp = posixtowin(wp, m);
        return wp;
    }
    else if (sc == L';') {
        pa = splitpath(ps, sc);

        if (pa != NULL) {
            for (i = 0; pa[i] != NULL; i++) {
                m = isposixpath(pa[i]);
                if (m == 0)
                    pa[i] = wcleanpath(pa[i]);
                else
                    pa[i] = posixtowin(pa[i], m);
            }
        }
    }
    else {
        pa = splitpath(ps, sc);

        if (pa != NULL) {
            for (i = 0; pa[i] != NULL; i++) {
                m = isposixpath(pa[i]);
                if (m == 0) {
                    xafree(pa);
                    return xwcsdup(ps);
                }
                else {
                    pa[i] = posixtowin(pa[i], m);
                }
            }
        }
    }
    if (pa != NULL) {
        wp = mergepath(pa);
        xafree(pa);
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
    if (fh == INVALID_HANDLE_VALUE)
        return NULL;

    while (buf == NULL) {
        buf = xwalloc(siz);
        len = GetFinalPathNameByHandleW(fh, buf, siz, VOLUME_NAME_DOS);
        if (len == 0) {
            CloseHandle(fh);
            xmfree(buf);
            return NULL;
        }
        if (len > siz) {
            xmfree(buf);
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

static wchar_t *xsearchexe(const wchar_t *name)
{
    DWORD     n;
    wchar_t   b[CYGWRUN_PATH_MAX];
    wchar_t  *r = NULL;

    n = SearchPathW(NULL, name, L".exe", CYGWRUN_PATH_MAX, b, NULL);
    if ((n > 8) && (n < CYGWRUN_PATH_MAX))
        r = getrealpathname(b, 0);
    return r;
}

static BOOL WINAPI consolehandler(DWORD ctrl)
{
    switch (ctrl) {
        case CTRL_LOGOFF_EVENT:
        break;
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
            if (cprocess) {
                if (WaitForSingleObject(cprocess, CYGWRUN_CRTL_C_WAIT) == WAIT_OBJECT_0)
                    break;
            }
            /* fall through */
        case CTRL_CLOSE_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            if (conevent && cprocess)
                SignalObjectAndWait(conevent, cprocess, CYGWRUN_CRTL_S_WAIT, FALSE);
        break;
        default:
            return FALSE;
        break;
    }
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
        if ((e.th32ParentProcessID == pid) && (xwcsicmp(e.szExeFile, L"CONHOST.EXE") != 0)) {
            pa[p].h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_TERMINATE | SYNCHRONIZE,
                                  FALSE, e.th32ProcessID);
            if (pa[p].h) {
                pa[p].n = 0;
                pa[p].i = e.th32ProcessID;
                p++;
                n++;
            }
        }
    } while (Process32NextW(sh, &e));
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

static void killproctree(DWORD pid)
{
    int i;
    int n;
    xprocinfo *pa;

    pa = xcalloc(CYGWRUN_KILL_SIZE, sizeof(xprocinfo));
    n  = getproctree(pa, pid, CYGWRUN_KILL_DEPTH, CYGWRUN_KILL_SIZE);
    for (i = n - 1; i >= 0; i--) {
        DWORD s = 0;

        if (pa[i].n)
            s = WaitForSingleObject( pa[i].h, CYGWRUN_KILL_TIMEOUT);
        if (s || !GetExitCodeProcess(pa[i].h, &s))
            s =  STILL_ACTIVE;
        if (s == STILL_ACTIVE)
            TerminateProcess(pa[i].h, CYGWRUN_SIGTERM);

        CloseHandle(pa[i].h);
    }
    xmfree(pa);
}

static wchar_t *getcygwinroot(void)
{
    DWORD    n;
    wchar_t  b[MAX_PATH];
    wchar_t *r = NULL;

    n = SearchPathW(NULL, L"cygwin1.dll", NULL, MAX_PATH, b, NULL);
    if ((n > 20) && (n < MAX_PATH)) {
        b[n - 16] = 0;
        r = xwcsdup(b);
        r = wcleanpath(r);
    }
    else {
        n = MAX_PATH * 2;
        if (RegGetValueW(HKEY_LOCAL_MACHINE,
                         L"Software\\Cygwin\\setup",
                         L"rootdir",
                         RRF_RT_REG_SZ,
                         NULL,
                         b, &n) == ERROR_SUCCESS) {
            r = xwcsdup(b);
            r = wcleanpath(r);
        }
    }
    return r;
}

static int initenvironment(const char **envp)
{
    const char **a;
    const char *ep;
    const char *ev;
    char  *ed;
    size_t n = 0;
    int    i;

    a = envp;
    while (*a != NULL) {
        n++;
        a++;
    }
    systemenvc = 0;
    systemenvn = xsaalloc(n + 1);
    systemenvv = xsaalloc(n + 1);

    while (*envp) {
        ep = *envp;
        if ((ep[0] == '=') || (ep[0] == '!')) {
            envp++;
            continue;
        }
        if ((ep[0] == '_') && (ep[1] == '=')) {
            envp++;
            continue;
        }
        n = xstrchrn(ep, '=');
        if (n == 0)
            return CYGWRUN_EBADENV;
        ev = ep + n + 1;
        if (IS_EMPTY_STR(ev))
            return CYGWRUN_EEMPTY;
        for (i = 0; unsetvars[i]; i++) {
            if (xstrnicmp(ep, unsetvars[i], n) == 0) {
                ev = NULL;
                break;
            }
        }
        if (ev == NULL) {
            envp++;
            continue;
        }
        for (i = 0; configvars[i]; i++) {
            if (xstrnicmp(ep, configvars[i], n) == 0) {
                if (configvals[i]) {
                    /* Multiple variables */
                    return CYGWRUN_EALREADY;
                }
                else {
                    configvals[i] = ev;
                    ev = NULL;
                    break;
                }
            }
        }
        if (ev == NULL) {
            envp++;
            continue;
        }
        ed    = xstrdup(ep);
        ed[n] = '\0';
        systemenvn[systemenvc] = ed;
        systemenvv[systemenvc] = ed + n + 1;
        systemenvc++;
        envp++;
    }
    return 0;
}

static int setupenvironment(void)
{
    int i;
    int j;

    xenvcount = 0;
    xenvvars  = xwaalloc(systemenvc + 3);
    xenvvals  = xwaalloc(systemenvc + 3);
    for (i = 0; i < systemenvc; i++) {
        char *es = systemenvn[i];

        if (adelenvv) {
            for (j = 0; adelenvv[j]; j++) {
                if (xstrimatch(es, adelenvv[j]) == 0) {
                    xmfree(es);
                    es = NULL;
                    break;
                }
            }
        }
        if (es == NULL)
            continue;
        xenvvars[xenvcount] = xmbstowcs(es);
        xenvvals[xenvcount] = xmbstowcs(systemenvv[i]);
        xenvcount++;
        xmfree(es);
    }
    xenvvars[xenvcount] = xwcsdup(L"PATH");
    xenvvals[xenvcount] = posixpath;
    xenvcount++;
    xenvvars[xenvcount] = xwcsdup(L"TEMP");
    xenvvals[xenvcount] = xmbstowcs(configvals[CCYGWIN_TEMP]);
    xenvcount++;
    xenvvars[xenvcount] = xwcsdup(L"TMP");
    xenvvals[xenvcount] = xmbstowcs(configvals[CCYGWIN_TMP]);
    xenvcount++;
    xmfree(systemenvn);
    xmfree(systemenvv);
    return 0;
}

static int runprogram(int argc, wchar_t **argv)
{
    int      i;
    int      m;
    size_t   n;
    DWORD    rc = 0;
    wchar_t *cmdblk = NULL;
    wchar_t *envblk = NULL;
    wchar_t *cmdexe = NULL;

    PROCESS_INFORMATION cp;
    STARTUPINFOW si;

    for (i = 1; i < argc; i++) {
        wchar_t *v;
        wchar_t *a = argv[i];

        if (*a == L'\'') {
            if (argv[0] != zerowcs)
                continue;
            n = xwcslen(a);
            if (a[n - 1] == L'\'') {
                v = xwalloc(n - 2);
                wmemcpy(v, a + 1, n - 2);
                xmfree(a);
                argv[i] = v;
            }
            continue;
        }
        v = cmdoptionval(a);
        if (v != NULL) {
            /**
             * In case the argv is [-|--]argument=["]value["]
             * evaluate the value as environment variable.
             */
            wchar_t  qc = 0;
            wchar_t *qp = NULL;
            if (v[0] == L'"') {
                /* We have quoted value */
                n = xwcslen(v) - 1;
                if ((n > 1) && (v[n] == v[0])) {
                    qc = v[0];
                    qp = v + n;
                    *(qp)  = 0;
                    *(v++) = 0;
                }
                else {
                    /**
                     * Found unterminated quote
                     * eg. -a="b
                     * Skip argument
                     */
                    continue;
                }
            }
            m = isanypath(1, v);
            if (m != 0) {
                wchar_t *p = pathstowin(v);
                if (qp == NULL)
                    v[0] = 0;
                argv[i]  = xwcsconcat(a, p, qc);
                xmfree(a);
                xmfree(p);
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
    if (argv[0] == zerowcs) {
        for (i = 1; i < argc; i++) {
            char *u = xwcstombs(argv[i]);
            if (i > 1)
                fputc('\n', stdout);
            fputs(u, stdout);
            xmfree(u);
        }
        return 0;
    }
    for (i = 0; i < xenvcount; i++) {
        int j;
        wchar_t *v = xenvvals[i];

        if (xenvvars[i] == zerowcs)
            continue;
        for (j = 0; askipenv[j]; j++) {
            if (xwcsimatch(xenvvars[i], askipenv[j]) == 0) {
                j = -1;
                break;
            }
        }
        if (j < 0)
            continue;
        m = isanypath(1, v);
        if (m != 0) {
            xenvvals[i] = pathstowin(v);
            xmfree(v);
        }
    }
    conevent = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (conevent == NULL)
        return CYGWRUN_FAILED;
    cmdexe  = xwcsdup(argv[0]);
    argv[0] = xwcsquote(argv[0]);
    for (i = 1; i < argc; i++)
        argv[i] = xquotearg(argv[i]);
    cmdblk = warraytomsz(argc, argv, L' ');
    envblk = getenvblock();

    memset(&cp, 0, sizeof(PROCESS_INFORMATION));
    memset(&si, 0, sizeof(STARTUPINFOW));
    si.cb = (DWORD)sizeof(STARTUPINFOW);

    SetConsoleCtrlHandler(NULL, FALSE);
    if (!CreateProcessW(cmdexe,
                        cmdblk,
                        NULL, NULL, TRUE,
                        CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT,
                        envblk, NULL,
                       &si, &cp)) {
        rc = CYGWRUN_FAILED;
    }
#if CYGWRUN_USE_MEMFREE
    xmfree(cmdexe);
    xmfree(envblk);
    xmfree(cmdblk);
#endif
    if (rc == 0) {
        HANDLE wh[2];
        DWORD  ws;

        cprocess = cp.hProcess;
        SetConsoleCtrlHandler(consolehandler, TRUE);
        ResumeThread(cp.hThread);
        CloseHandle(cp.hThread);
        wh[0] = cprocess;
        wh[1] = conevent;
        /**
         * Wait for child to finish
         */
        rc = STILL_ACTIVE;
        ws = WaitForMultipleObjects(2, wh, FALSE, INFINITE);
        if (ws != WAIT_OBJECT_0) {
            /**
             * Process did not finish within timeout
             * or the console event was signaled.
             * Rise CTRL+C signal
             */
            SetConsoleCtrlHandler(NULL, TRUE);
            GenerateConsoleCtrlEvent(0, CTRL_C_EVENT);
            SetConsoleCtrlHandler(NULL, FALSE);
            ws = WaitForSingleObject(wh[0], CYGWRUN_CRTL_C_WAIT);
            if (ws == WAIT_OBJECT_0)
                rc = CYGWRUN_SIGINT;
        }
        if (rc == STILL_ACTIVE && GetExitCodeProcess(cprocess, &rc)) {
            if ((rc != STILL_ACTIVE) && (rc > CYGWRUN_ERRMAX))
                rc = CYGWRUN_ERRMAX;
        }
        if (rc == STILL_ACTIVE) {
            killproctree(cp.dwProcessId);
            TerminateProcess(cprocess, CYGWRUN_SIGTERM);
            rc = CYGWRUN_SIGTERM;
        }
        SetConsoleCtrlHandler(consolehandler, FALSE);
        CloseHandle(cprocess);
    }
    CloseHandle(conevent);
    return rc;
}

static int version(void)
{
#if CYGWRUN_ISDEV_VERSION
    fputs(CYGWRUN_NAME " version " CYGWRUN_VERSION_ALL " (dev)", stdout);
#else
    fputs(CYGWRUN_NAME " version " CYGWRUN_VERSION_STR, stdout);
#endif
    return 0;
}

#define __NEXT_ARG()   --argc; ++argv; optarg = *argv
int main(int argc, const char **argv, const char **envp)
{
    int         i;
    int         rv;
    wchar_t   **dupargv;
    wchar_t    *wparam;
    wchar_t    *eparam;
    char       *sparam;
    const char *optarg;
#if CYGWRUN_HAVE_CMDOPTS
    const char *scmdopt = NULL;
    const char *ucmdopt = NULL;
#endif
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX | SEM_NOGPFAULTERRORBOX);
    if (argc < 2)
        return CYGWRUN_ENOEXEC;
    __NEXT_ARG();
    if ((argc == 1) && (optarg[0] == '-') && (optarg[1] == 'v') && (optarg[2] == '\0'))
        return version();
#if CYGWRUN_USE_HEAPAPI
#if CYGWRUN_USE_PRIVATE_HEAP
    memheap = HeapCreate(0, 0, 0);
#else
    memheap = GetProcessHeap();
#endif
    if (memheap == NULL)
        return CYGWRUN_ENOMEM;
#endif
    posixroot = getcygwinroot();
    if (posixroot == NULL)
        return CYGWRUN_ENOSYS;
    rv = initenvironment(envp);
    if (rv)
        return rv;
    if ((configvals[CCYGWIN_TEMP] == NULL) ||
        (configvals[CCYGWIN_TMP]  == NULL))
        return CYGWRUN_EBADPATH;
#if CYGWRUN_HAVE_CMDOPTS
    while (*optarg == '-') {
        int opt = *++optarg;

        if (!xisalpha(opt) || (*++optarg != '='))
            return CYGWRUN_EPARAM;
        ++optarg;
        if (IS_EMPTY_STR(optarg))
            return CYGWRUN_EPARAM;
        switch (opt) {
            case 's':
                if (scmdopt)
                    return CYGWRUN_EALREADY;
                scmdopt = optarg;
            break;
            case 'u':
                if (ucmdopt)
                    return CYGWRUN_EALREADY;
                ucmdopt = optarg;
            break;
            default:
                return CYGWRUN_EINVAL;
            break;
        }
        __NEXT_ARG();
    }
#endif
    if (argc < 1)
        return CYGWRUN_ENOEXEC;
    sparam   = xstrdup(configvals[CYGWRUN_SKIP]);
#if CYGWRUN_HAVE_CMDOPTS
    sparam   = xstrappend(sparam, scmdopt,  ',');
#endif
    sparam   = xstrappend(sparam, sskipenv, ',');
    wparam   = xmbstowcs(sparam);
    askipenv = wcstoarray(wparam, L',');
#if CYGWRUN_USE_MEMFREE
    xmfree(wparam);
    xmfree(sparam);
#endif
    sparam   = xstrdup(configvals[CYGWRUN_UNSET]);
#if CYGWRUN_HAVE_CMDOPTS
    sparam   = xstrappend(sparam, ucmdopt,  ',');
#endif
    adelenvv = strtoarray(sparam, ',');
#if CYGWRUN_USE_MEMFREE
    xmfree(sparam);
#endif
    eparam = xmbstowcs(configvals[CYGWRUN_PATH]);
    if (eparam == NULL)
        eparam = xmbstowcs(configvals[CCYGWIN_PATH]);
    if (eparam == NULL)
        return CYGWRUN_ENOENT;
    posixpath = pathstowin(eparam);
    if (posixpath== NULL)
        return CYGWRUN_EBADPATH;
#if CYGWRUN_USE_MEMFREE
    xmfree(eparam);
#endif
    SetEnvironmentVariableW(L"PATH", posixpath);
    rv = setupenvironment();
    if (rv)
        return rv;
    dupargv = xwaalloc(argc + 1);
    if ((*optarg == '.') && (*(optarg + 1) == '\0')) {
        dupargv[0] = zerowcs;
        if (argc < 2)
            return CYGWRUN_ENOEXEC;
    }
    else {
        wparam = pathtowin(xmbstowcs(optarg));
        if (wparam == NULL)
            return CYGWRUN_ENOEXEC;
        eparam = getrealpathname(wparam, 0);
        if (eparam == NULL) {
            SetSearchPathMode(BASE_SEARCH_PATH_DISABLE_SAFE_SEARCHMODE);
            eparam = xsearchexe(wparam);
        }
        if (eparam == NULL)
            return CYGWRUN_ENOEXEC;
        xmfree(wparam);
        dupargv[0] = eparam;
    }
    for (i = 1; i < argc; i++)
        dupargv[i] = xmbstowcs(argv[i]);
    rv = runprogram(i, dupargv);
#if CYGWRUN_USE_MEMFREE && CYGWRUN_ISDEV_VERSION
    xafree(xenvvals);
    xafree(xenvvars);
    xafree(dupargv);
    xafree(askipenv);
    xafree(adelenvv);
    xmfree(posixroot);
    if (xnalloc != xnmfree)
        fprintf(stderr, "\nAllocated: %llu\n"
                        "alloc    : %d\n"
                        "free     : %d\n",
                        xzalloc, xnalloc, xnmfree);
#endif
#if CYGWRUN_USE_HEAPAPI
#if CYGWRUN_USE_MEMFREE && CYGWRUN_ISDEV_VERSION
    if (xzalloc != xzmfree)
        fprintf(stderr, "\nAllocated: %llu\n"
                        "Free     : %llu\n",
                        xzalloc, xzmfree);
#endif
#if CYGWRUN_USE_PRIVATE_HEAP
    HeapDestroy(memheap);
#endif
#endif
    return rv;
}
