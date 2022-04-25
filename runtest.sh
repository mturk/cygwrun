#!/bin/sh
#
# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
#
b="`basename $0`"
d="`dirname $0`"
srcdir="`cd \"$d\" && pwd`"

case "`uname -s`" in
  CYGWIN*)
    phost=cygwin
  ;;
  *)
    echo "Unknown `uname`"
    echo "Test suite can run only inside cygwin"
    exit 1
  ;;
esac

xbexit()
{
    e=$1; shift;
    echo "$@" 1>&2
    exit $e
}

_cygwrun=$srcdir/x64/cygwrun.exe
test -x "$_cygwrun" || xbexit 1 "Cannot find cygwrun.exe in \`$srcdir/x64'"
_dumpargs=$srcdir/x64/dumpargs.exe
test -x "$_dumpargs" || xbexit 1 "Cannot find dumpargs.exe in \`$srcdir/x64'"
_dumpenvp=$srcdir/x64/dumpenvp.exe
test -x "$_dumpenvp" || xbexit 1 "Cannot find dumpenvp.exe in \`$srcdir/x64'"

echo "Running cygwrun test suite on: $phost"

export POSIX_ROOT="C:\\_not\\a/directory//"
v=/tmp
rv="`$_cygwrun -p $v`"
test "x$rv" = "xC:\\_not\\a\\directory\\tmp" || xbexit 1 "Failed #1.1: \`$rv'"
unset POSIX_ROOT

rr="`$_cygwrun -r c:/cygwin64 -p /`"
rv="`$_cygwrun -p /`"
test "x$rv" = "x$rr" || xbexit 1 "Failed #1.2: \`$rv -- $rr'"

tmpdir="`$_cygwrun -p /tmp`"
test $? -ne 0 && xbexit 1 "Failed #2.1"

usrdir="`$_cygwrun -p /usr`"
test $? -ne 0 && xbexit 1 "Failed #2.2"

rootdir="`$_cygwrun -p /`"
test $? -ne 0 && xbexit 1 "Failed #2.3"

export FOO=bar
rv="`$_cygwrun -e FOO`"
test "x$rv" = "xFOO=bar" || xbexit 1 "Failed #3.1: \`$rv'"

export FOO=/Fo:/tmp:/usr
rv="`$_cygwrun -e FOO`"
test "x$rv" = "xFOO=/Fo:/tmp:/usr" || xbexit 1 "Failed #3.2: \`$rv'"


export FOO="/tmp/foo/bar:/usr/local"
rv="`$_cygwrun -e FOO`"
test "x$rv" = "xFOO=$tmpdir\\foo\\bar;$usrdir\\local" || xbexit 1 "Failed #4.1: \`$rv'"

export FOO="-unknown:/tmp/foo/bar:/usr/local"
rv="`$_cygwrun -e FOO`"
test "x$rv" = "xFOO=$FOO" || xbexit 1 "Failed #5.1: \`$rv'"
export FOO="/tmp/foo/bar::/unknown:/usr/local"
rv="`$_cygwrun -e FOO`"
test "x$rv" = "xFOO=$FOO" || xbexit 1 "Failed #5.2: \`$rv'"
rv="`$_cygwrun -f -e FOO`"
test "x$rv" = "xFOO=$tmpdir\\foo\\bar;$rootdir\\unknown;$usrdir\\local" || xbexit 1 "Failed #5.3: \`$rv'"
rv="`$_cygwrun -p -I=/tmp/foo`"
test "x$rv" = "x-I=$tmpdir\\foo" || xbexit 1 "Failed #5.4: \`$rv'"

export FOO="/usr/a::/usr/b"
rv="`$_cygwrun -e FOO`"
test "x$rv" = "xFOO=$usrdir\\a;$usrdir\\b" || xbexit 1 "Failed #6.1: \`$rv'"

export FOO="/usr/a:$abcd0123456789::/usr/b"
rv="`$_cygwrun -e FOO`"
test "x$rv" = "xFOO=$usrdir\\a;$usrdir\\b" || xbexit 1 "Failed #6.2: \`$rv'"

rv="`$_cygwrun -p /libpath:./tmp/foo`"
test "x$rv" = "x/libpath:.\\tmp\\foo" || xbexit 1 "Failed #7.1: \`$rv'"

rv="`$_cygwrun -p ./tmp`"
test "x$rv" = "x.\\tmp" || xbexit 1 "Failed #8.1: \`$rv'"

rv="`$_cygwrun -p ../tmp/foo`"
test "x$rv" = "x..\\tmp\\foo" || xbexit 1 "Failed #8.2: \`$rv'"

rv="`$_cygwrun -p ../tmp:/foo`"
test "x$rv" = "x../tmp:/foo" || xbexit 1 "Failed #8.3: \`$rv'"

rv="`$_cygwrun -p .../tmp`"
test "x$rv" = "x.../tmp" || xbexit 1 "Failed #8.4: \`$rv'"

rv="`$_cygwrun $_dumpargs -f1=../tmp/foo`"
test "x$rv" = "x-f1=..\\tmp\\foo" || xbexit 1 "Failed #9.1: \`$rv'"

export FOO="/usr/a::/usr/b"
rv="`$_cygwrun $_dumpenvp FOO`"
test "x$rv" = "xFOO=$usrdir\\a;$usrdir\\b" || xbexit 1 "Failed #10.1: \`$rv'"


echo "All tests passed!"
exit 0
