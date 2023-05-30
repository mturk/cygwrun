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
_dumpargs=./x64/dumpargs
test -x "$_dumpargs" || xbexit 1 "Cannot find dumpargs.exe in \`./x64'"
_dumpenvp=./x64/dumpenvp
test -x "$_dumpenvp" || xbexit 1 "Cannot find dumpenvp.exe in \`./x64'"

echo "Running cygwrun test suite on: $phost"

export POSIX_ROOT="c:\\windows/"
v=/tmp
rv="`$_cygwrun -p $v`"
test "x$rv" = "xc:\\windows\\tmp" || xbexit 1 "Failed #1.1: \`$rv'"
unset POSIX_ROOT

rootdir="`$_cygwrun -p /`"
test $? -ne 0 && xbexit 1 "Failed #1.2"
tmpdir="`$_cygwrun -p /tmp`"
test $? -ne 0 && xbexit 1 "Failed #1.3"
usrdir="`$_cygwrun -p /usr`"
test $? -ne 0 && xbexit 1 "Failed #1.4"
bindir="`$_cygwrun -p /bin`"
test $? -ne 0 && xbexit 1 "Failed #1.5"

rv="`$_cygwrun -r $rootdir\\\\ -p /`"
test "x$rv" = "x$rootdir" || xbexit 1 "Failed #2.1: \`$rv'"
export CYGWIN_ROOT="$rootdir//; ;"
rv="`$_cygwrun -p /`"
test "x$rv" = "x$rootdir" || xbexit 1 "Failed #2.2: \`$rv'"
unset CYGWIN_ROOT
rv="`$_cygwrun -r whatever/path/ -p /usr`"
test "x$rv" = "xwhatever\\path\\usr" || xbexit 1 "Failed #2.3: \`$rv'"


rv="`$_cygwrun -f -p /usr=/tmp/foo`"
test "x$rv" = "x$usrdir=\\tmp\\foo" || xbexit 1 "Failed #3.1: \`$rv'"
rv="`$_cygwrun -f -p /usr:/tmp/foo`"
test "x$rv" = "x$usrdir:\\tmp\\foo" || xbexit 1 "Failed #3.2: \`$rv'"
rv="`$_cygwrun -p I=/tmp/foo`"
test "x$rv" = "xI=$tmpdir\\foo" || xbexit 1 "Failed #3.3: \`$rv'"
rv="`$_cygwrun -p - -I=/tmp/foo`"
test "x$rv" = "x-I=$tmpdir\\foo" || xbexit 1 "Failed #3.4: \`$rv'"
rv="`$_cygwrun -p -- --I=/tmp/foo`"
test "x$rv" = "x--I=$tmpdir\\foo" || xbexit 1 "Failed #3.5: \`$rv'"
rv="`$_cygwrun -p --I=/tmp/foo`"
test "x$rv" = "x--I=$tmpdir\\foo" || xbexit 1 "Failed #3.6: \`$rv'"
rv="`$_cygwrun -p I=/tmp/foo:/tmp/bar`"
test "x$rv" = "xI=$tmpdir\\foo;$tmpdir\\bar" || xbexit 1 "Failed #3.7: \`$rv'"
rv="`$_cygwrun -p \"I=/tmp/foo::  :/tmp/bar\"`"
test "x$rv" = "xI=$tmpdir\\foo;$tmpdir\\bar" || xbexit 1 "Failed #3.8: \`$rv'"
rv="`$_cygwrun -p I=\"\\\"/tmp/\"\\\"`"
test "x$rv" = "xI=\"$tmpdir\"" || xbexit 1 "Failed #3.9: \`$rv'"
rv="`$_cygwrun -p I=\"\\\"/tmp/\"`"
test "x$rv" = "xI=\"/tmp/" || xbexit 1 "Failed #3.10: \`$rv'"
rv="`$_cygwrun -p I=\"\\\"./tmp\"\\\"`"
test "x$rv" = "xI=\".\\tmp\"" || xbexit 1 "Failed #3.11: \`$rv'"
rv="`$_cygwrun -p I=\"\\\"./tmp:./bar\"\\\"`"
test "x$rv" = "xI=\".\\tmp;.\\bar\"" || xbexit 1 "Failed #3.12: \`$rv'"

rv="`$_cygwrun -p ./tmp`"
test "x$rv" = "x.\\tmp" || xbexit 1 "Failed #4.1: \`$rv'"
rv="`$_cygwrun -p ../tmp/foo`"
test "x$rv" = "x..\\tmp\\foo" || xbexit 1 "Failed #4.2: \`$rv'"
rv="`$_cygwrun -p .../tmp`"
test "x$rv" = "x.../tmp" || xbexit 1 "Failed #4.3: \`$rv'"
rv="`$_cygwrun -p \"\\\"./tmp\"\\\"`"
test "x$rv" = "x\"./tmp\"" || xbexit 1 "Failed #4.4: \`$rv'"
rv="`$_cygwrun -p ..`"
test "x$rv" = "x.." || xbexit 1 "Failed #4.5: \`$rv'"

rv="`$_cygwrun $_dumpargs -f1=../tmp/foo`"
test "x$rv" = "x-f1=..\\tmp\\foo" || xbexit 1 "Failed #5.1: \`$rv'"
rv="`$_cygwrun $_dumpargs --f=../tmp/foo`"
test "x$rv" = "x--f=..\\tmp\\foo" || xbexit 1 "Failed #5.2: \`$rv'"
rv="`$_cygwrun $_dumpargs f=../tmp/foo`"
test "x$rv" = "xf=..\\tmp\\foo" || xbexit 1 "Failed #5.3: \`$rv'"
rv="`$_cygwrun -w ./x64 dumpargs f=../tmp/foo`"
test "x$rv" = "xf=..\\tmp\\foo" || xbexit 1 "Failed #5.4: \`$rv'"

export FOO="/usr/a::/usr/b:"
rv="`$_cygwrun $_dumpenvp FOO`"
test "x$rv" = "xFOO=$usrdir\\a;$usrdir\\b" || xbexit 1 "Failed #6.1: \`$rv'"
export FOO="/usr:/bin: :../d"
export BAR="/usr/bin"
rv="`$_cygwrun $_dumpenvp FOO`"
test "x$rv" = "xFOO=$usrdir;$bindir;..\\d" || xbexit 1 "Failed #6.2: \`$rv'"
rv="`$_cygwrun -n FOO $_dumpenvp FOO`"
test "x$rv" = "xFOO=$FOO" || xbexit 1 "Failed #6.3: \`$rv'"
rv="`$_cygwrun -n \" FOO  ,,\" $_dumpenvp FOO`"
test "x$rv" = "xFOO=$FOO" || xbexit 1 "Failed #6.4: \`$rv'"
rv="`$_cygwrun -n \"FOO , ,    BAR\" $_dumpenvp BAR`"
test "x$rv" = "xBAR=$BAR" || xbexit 1 "Failed #6.5: \`$rv'"

rv="`$_cygwrun $srcdir/test/echoargs.bat /tmp /usr/a | tr -d '\r'`"
test "x$rv" = "x$tmpdir $usrdir\\a" || xbexit 1 "Failed #7.1: \`$rv'"
rv="`$_cygwrun $srcdir/test/echoargs.bat \"/tmp/some location\" | tr -d '\r'`"
test "x$rv" = "x\"$tmpdir\\some location\"" || xbexit 1 "Failed #7.2: \`$rv'"


echo "All tests passed!"
exit 0
