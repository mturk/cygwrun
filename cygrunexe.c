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
 * limitations uSder the License.
 *
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

#include "cygrunexe.h"

#define IS_PSW(c)         ((c) == L'/' || (c) == L'\\')
#define IS_EMPTY_WCS(_s)  (((_s) == 0) || (*(_s) == L'\0'))

#if defined(_TEST_MODE)
#undef _HAVE_DEBUG_OPTION
#endif

#if defined(_HAVE_DEBUG_OPTION)
static int      debug     = 0;
#endif
static int      execmode  = _P_WAIT;
static wchar_t *posixroot = 0;

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
    0
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
    0
};

static const wchar_t *removeenv[] = {
    L"CYGWIN_ROOT=",
    L"INFOPATH=",
    L"MANPATH=",
    L"OLDPWD=",
    L"ORIGINAL_PATH=",
    L"ORIGINAL_TEMP=",
    L"ORIGINAL_TMP=",
    L"PATH=",
    L"POSIX_ROOT=",
    L"PS1=",
    L"SHELL=",
    L"TERM=",
    L"_=",
    L"!::=",
    L"!;=",
    0
};

static const wchar_t *posixrenv[] = {
    L"CYGWIN_ROOT",
    L"POSIX_ROOT",
    L"HOMEPATH",
    0
};


static int usage(int rv)
{
    FILE *os = rv == 0 ? stdout : stderr;
    fputs("\nUsage " PROJECT_NAME " [OPTIONS]... PROGRAM [ARGUMENTS]...\n", os);
    fputs("Execute PROGRAM [ARGUMENTS]...\n\nOptions are:\n", os);
    fputs(" -a        Use async execution mode.\n", os);
#if defined(_HAVE_DEBUG_OPTION)
    fputs(" -d        print replaced arguments and environment\n", os);
    fputs("           instead executing PROGRAM.\n", os);
#endif
    fputs(" -v        print version information and exit.\n", os);
    fputs(" -h        print this screen and exit.\n", os);
    fputs(" -w <DIR>  change working directory to DIR before calling PROGRAM\n", os);
    fputs(" -r <DIR>  use DIR as posix root\n\n", os);
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
    fwprintf(stderr, L"Unknown option: %s\n\n", arg);
    return usage(EINVAL);
}

static void *xmalloc(size_t size)
{
    void *p = calloc(size, 1);
    if (p == 0) {
        _wperror(L"xmalloc");
        _exit(1);
    }
    return p;
}

static wchar_t *xwalloc(size_t size)
{
    wchar_t *p = (wchar_t *)calloc(size, sizeof(wchar_t));
    if (p == 0) {
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
    if (m != 0)
        free(m);
}

static void waafree(wchar_t **array)
{
    wchar_t **ptr = array;

    if (array == 0)
        return;
    while (*ptr != 0)
        xfree(*(ptr++));
    free(array);
}

static wchar_t *xwcsdup(const wchar_t *s)
{
    wchar_t *p;
    size_t   n;

    if (IS_EMPTY_WCS(s))
        return 0;
    n = wcslen(s);
    p = xwalloc(n + 2);
    wmemcpy(p, s, n);
    return p;
}

static wchar_t *xgetenv(const wchar_t *s)
{
    wchar_t *d;

    if (IS_EMPTY_WCS(s))
        return 0;
    d = _wgetenv(s);
    if (IS_EMPTY_WCS(d))
        return 0;
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
        return 0;
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
    for ( ; *wexp != L'\0'; wstr++, wexp++) {
        if (*wstr == L'\0' && *wexp != L'*')
            return -1;
        switch (*wexp) {
            case L'*':
                wexp++;
                while (*wexp == L'*') {
                    /* Skip multiple stars */
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
                if (*wstr > 127 || isalpha(*wstr) == 0)
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
        /* Check for .[/] or ..[/] */
        return isdotpath(str);
    }

    if (str[1] == L'\0')
        return 301;
    if (wcscmp(str, L"/dev/null") == 0)
        return 302;
    if (wcschr(str + 1, L'/') == 0) {
        /* No additional slashes */
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
    if (iswinpath(s) || isposixpath(s))
        return 0;
    while (*(s++) != L'\0') {
        if (IS_PSW(*s) || iswspace(*s))
            return 0;
        if (*s == L'=' || *s == L':')
            return s + 1;
    }
    return 0;
}

static int envsort(const void *arg1, const void *arg2)
{
    return _wcsicoll(*((wchar_t **)arg1), *((wchar_t **)arg2));
}

static void xwinpathsep(wchar_t *s)
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
    const wchar_t *b;
    const wchar_t *e;

    e = b = s;
    while (*e != L'\0') {
        if (*(e++) == L':')
            x++;
    }
    sa = waalloc(x + 1);
    if (x > 0) {
        while ((e = wcschr(b, L':')) != 0) {
            wchar_t *p;
            int cn   = 1;
            size_t n = (size_t)(e - b);

            p = xwalloc(n + 2);
            if (n == 0) {
                /* Preserve leading or multiple colons */
                p[n] = L':';
            }
            else {
                wmemcpy(p, b, n);
                /* Is the previous token path or flag */
                if (isposixpath(p)) {
                    while (*(e + cn) == L':') {
                        /* Drop multiple trailing colons */
                        cn++;
                    }
                }
                else {
                    /* Preserve colon for unresolved paths */
                    p[n] = L':';
                }
            }
            sa[c++] = p;
            b = s = e + cn;
            if (*b == L'\0')
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

    for (i = 0; pp[i] != 0; i++) {
        len += wcslen(pp[i]) + 1;
    }
    r = p = xwalloc(len + 2);
    for (i = 0; pp[i] != 0; i++) {
        len = wcslen(pp[i]);
        if (len > 0) {
            if (sc)
                *(p++) = L';';
            /* do not add semicolon before next path */
            if (pp[i][len - 1] == L':')
                sc = 0;
            else
                sc = 1;
            wmemcpy(p, pp[i], len);
            p += len;
        }
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
        if (IS_PSW(s[i]) || s[1] == L';')
            s[i] = L'\0';
        else
            break;
    }
}

static wchar_t *posix2win(wchar_t *pp)
{
    int m;
    wchar_t *rv;
    wchar_t  windrive[] = { 0, L':', L'\\', 0};

    if (wcschr(pp, L'/') == 0)
        return pp;
    /**
     * Check for special paths
     */
    m = isposixpath(pp);
    if (m == 0) {
        /* Not a posix path */
        if (iswinpath(pp))
            xwinpathsep(pp);
        return pp;
    }
    else if (m == 100) {
        /* /cygdrive/x/... absolute path */
        windrive[0] = towupper(pp[10]);
        rv = xwcsconcat(windrive, pp + 12);
        xwinpathsep(rv + 3);
    }
    else if (m == 101) {
        /* /x/... msys2 absolute path */
        windrive[0] = towupper(pp[1]);
        if (windrive[0] != *posixroot)
            return pp;
        rv = xwcsconcat(windrive, pp + 3);
        xwinpathsep(rv + 3);
    }
    else if (m == 300) {
        xwinpathsep(pp);
        return pp;
    }
    else if (m == 301) {
        rv = xwcsdup(posixroot);
    }
    else if (m == 302) {
        rv = xwcsdup(L"NUL");
    }
    else {
        xwinpathsep(pp);
        rv = xwcsconcat(posixroot, pp);
    }
    xfree(pp);
    return rv;
}

static wchar_t *convert2win(const wchar_t *str)
{
    wchar_t *wp;

    if ((*str == L'\'') || (wcschr(str, L'/') == 0))
        return 0;
    if (iswinpath(str)) {
        wp = xwcsdup(str);
        xwinpathsep(wp);
    }
    else {
        int i, n;
        wchar_t **pa = splitpath(str, &n);

        for (i = 0; i < n; i++) {
            wchar_t *e = pa[i];
            pa[i] = posix2win(e);
            if (pa[i] == e) {
               break;
            }
            rmtrailingsep(pa[i]);
        }
        wp = mergepath(pa);
        waafree(pa);
    }
    return wp;
}

static wchar_t *getposixroot(wchar_t *r)
{
    if (r == 0) {
        const wchar_t **e = posixrenv;
        while (*e != 0) {
            if ((r = xgetenv(*e)) != 0)
                break;
            e++;
        }
    }
    if (r != 0) {
        rmtrailingsep(r);
        xwinpathsep(r);
        if (isalpha(*r & 0x7F))
            *r = towupper(*r);
    }
    return r;
}

static void __cdecl xstdinrw(void *p)
{
    unsigned char buf[BUFSIZ];
    int *fds = (int *)p;
    int nr;

    while ((nr = _read(fds[0], buf, BUFSIZ)) > 0) {
        _write(fds[1], buf, nr);
    }
}

static int posixmain(int argc, wchar_t **wargv, int envc, wchar_t **wenvp)
{
    int i, rc = 0;
    intptr_t rp;
    int orgstdin = -1;
    int stdinpipe[2];


#if defined(_HAVE_DEBUG_OPTION)
    if (debug) {
        wprintf(L"Posix root: %s\n\n", posixroot);
        wprintf(L"Arguments (%d):\n",  argc);
    }
#endif
    for (i = 0; i < argc; i++) {
        wchar_t *a = wargv[i];
#if defined(_HAVE_DEBUG_OPTION)
        if (debug)
            wprintf(L"[%2d] : %s\n", i, a);
#endif
        if (wcschr(a, L'/') == 0)
            continue;
        if (wcslen(a) > 3) {
            wchar_t *p;
            wchar_t *v = cmdoptionval(a);
            if (IS_EMPTY_WCS(v))
                p = convert2win(a);
            else
                p = convert2win(v);
            if (p != 0) {
                if (IS_EMPTY_WCS(v))
                    wargv[i] = p;
                else {
                    *v = L'\0';
                    wargv[i] = xwcsconcat(a, p);
                    xfree(p);
                }
                xfree(a);
#if defined(_HAVE_DEBUG_OPTION)
                if (debug)
                    wprintf(L"     * %s\n", wargv[i]);
#endif
            }
        }
    }

#if defined(_HAVE_DEBUG_OPTION)
    if (debug)
        wprintf(L"\nEnvironment variables (%d):\n", envc);
#endif
    for (i = 0; i < (envc - 1); i++) {
        wchar_t *p;
        wchar_t *e = wenvp[i];
#if defined(_HAVE_DEBUG_OPTION)
        if (debug)
            wprintf(L"[%2d] : %s\n", i, e);
#endif
        if ((p = wcschr(e, L'=')) != 0) {
            wchar_t *v = p + 1;
            if (wcschr(v, L'/') == 0)
                continue;
            if ((wcslen(v) > 3) && ((p = convert2win(v)) != 0)) {
                *v = L'\0';
                wenvp[i] = xwcsconcat(e, p);
                xfree(e);
                xfree(p);
#if defined(_HAVE_DEBUG_OPTION)
                if (debug)
                    wprintf(L"     * %s\n", wenvp[i]);
#endif
            }
        }
    }
#if defined(_HAVE_DEBUG_OPTION)
    if (debug) {
        wprintf(L"[%2d] : %s\n", i, wenvp[i]);
        return 0;
    }
#endif

    qsort((void *)wenvp, envc, sizeof(wchar_t *), envsort);
#if defined(_TEST_MODE)
    if (wcscmp(wargv[0], L"arg") == 0) {
        for (i = 1; i < argc; i++)
            _putws(wargv[i]);
        return 0;
    }
    if (wcscmp(wargv[0], L"env") == 0) {
        for (i = 0; i < envc; i++) {
            if (wargv[1] == 0 || strstartswith(wenvp[i], wargv[1]))
                _putws(wenvp[i]);
        }
        return 0;
    }
    fprintf(stderr, "unknown test %S .. use arg or env\n", wargv[0]);
    rc = EINVAL;
#else
    if (execmode == _P_NOWAIT) {
        if(_pipe(stdinpipe, BUFSIZ, O_NOINHERIT) == -1) {
            rc = errno;
            _wperror(L"Fatal error _pipe()");
            return rc;
        }
        orgstdin = _dup(_fileno(stdin));
        if(_dup2(stdinpipe[0], _fileno(stdin)) != 0) {
            rc = errno;
            _wperror(L"Fatal error _dup2()");
            return rc;
        }
        _close(stdinpipe[0]);
    }
    _flushall();
    rp = _wspawnvpe(execmode, wargv[0], wargv, wenvp);
    if (rp == (intptr_t)-1) {
        rc = errno;
        fwprintf(stderr, L"Cannot execute program: %s\nFatal error: %s\n\n",
                 wargv[0], _wcserror(rc));
        return usage(rc);
    }
    if (execmode == _P_NOWAIT) {
        /* Restore original stdin handle */
        if(_dup2(orgstdin, _fileno(stdin)) != 0) {
            rc = errno;
            _wperror(L"Fatal error _dup2()");
            return rc;
        }
        _close(orgstdin);
        /* Create stdin R/W thread */
        stdinpipe[0] = _fileno(stdin);
        _beginthread(xstdinrw, 0, (void *)stdinpipe);
        if (_cwait(&rc, rp, _WAIT_CHILD) == (intptr_t)-1) {
            rc = errno;
            fwprintf(stderr, L"Execute failed: %s\nFatal error: %s\n\n",
                    wargv[0], _wcserror(rc));
            return usage(rc);
        }
    }

#endif
    return rc;
}

int wmain(int argc, const wchar_t **wargv, const wchar_t **wenv)
{
    int i;
    wchar_t **dupwargv = 0;
    wchar_t **dupwenvp = 0;
    wchar_t *crp       = 0;
    wchar_t *cwd       = 0;
    wchar_t *opath;
    wchar_t  nnp[4]    = { L'\0', L'\0', L'\0', L'\0' };
    int dupenvc = 0;
    int dupargc = 0;
    int envc    = 0;
    int opts    = 1;

    if (argc < 2)
        return usage(1);
    if (wenv == 0)
        return invalidarg(L"missing environment");
    dupwargv = waalloc(argc);
    for (i = 1; i < argc; i++) {
        const wchar_t *p = wargv[i];
        if (IS_EMPTY_WCS(p)) {
            /**
             * We do not support empty arguments
             */
            return invalidarg(L"empty argument");
        }
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
                if (p[1] == L'\0' || p[2] != L'\0')
                    return invalidarg(p);
                switch (p[1]) {
                    case L'a':
                    case L'A':
                        execmode = _P_NOWAIT;
                    break;
#if defined(_HAVE_DEBUG_OPTION)
                    case L'd':
                    case L'D':
                        debug = 1;
                    break;
#endif
                    case L'h':
                    case L'H':
                    case L'?':
                        return usage(0);
                    break;
                    case L'r':
                    case L'R':
                        crp = nnp;
                    break;
                    case L'v':
                    case L'V':
                        return version();
                    break;
                    case L'w':
                    case L'W':
                        cwd = nnp;
                    break;
                    default:
                        return invalidarg(wargv[i]);
                    break;
                }
                continue;
            }
            opts = 0;
        }
        dupwargv[dupargc++] = xwcsdup(p);
    }
    if ((cwd == nnp) || (crp == nnp)) {
        fputs("Missing required parameter value\n\n", stderr);
        return usage(1);
    }
    if ((opath = xgetenv(L"PATH")) == 0) {
        fputs("Missing PATH environment variable\n\n", stderr);
        return usage(1);
    }
    rmtrailingsep(opath);
    if ((posixroot = getposixroot(crp)) == 0) {
        fputs("Cannot determine POSIX_ROOT\n\n", stderr);
        return usage(1);
    }
#if defined(_HAVE_DEBUG_OPTION)
    if (debug) {
        printf(PROJECT_NAME " version %s (%s)\n\n",
               PROJECT_VERSION_STR, __DATE__ " " __TIME__);
    }
#endif
    if (cwd != 0) {
        rmtrailingsep(cwd);
        cwd = posix2win(cwd);
        if (_wchdir(cwd) != 0) {
            i = errno;
            fwprintf(stderr, L"Invalid working directory: %s\nFatal error: %s\n\n",
                     cwd, _wcserror(i));
            return usage(i);
        }
    }
    while (wenv[envc] != 0) {
        if (IS_EMPTY_WCS(wenv[envc]))
            return invalidarg(L"empty environment variable");
        ++envc;
    }

    dupwenvp = waalloc(envc + 2);
    for (i = 0; i < envc; i++) {
        const wchar_t **e = removeenv;
        const wchar_t *p  = wenv[i];

        while (*e != 0) {
            if (strstartswith(p, *e)) {
                /**
                 * Skip private environment variable
                 */
                p = 0;
                break;
            }
            e++;
        }
        if (p != 0)
            dupwenvp[dupenvc++] = xwcsdup(p);
    }

    /**
     * Add additional environment variables
     */
    dupwenvp[dupenvc++] = xwcsconcat(L"PATH=", opath);
    xfree(opath);

    return posixmain(dupargc, dupwargv, dupenvc, dupwenvp);
}
