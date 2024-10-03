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

_cygwrun=$srcdir/.build/cygwrun.exe
test -x "$_cygwrun" || xbexit 1 "Cannot find cygwrun.exe in \`$srcdir/.build'"
_dumpargs=./.build/dumpargs
test -x "$_dumpargs" || xbexit 1 "Cannot find dumpargs.exe in \`./.build'"
_dumpenvp=./.build/dumpenvp
test -x "$_dumpenvp" || xbexit 1 "Cannot find dumpenvp.exe in \`./.build'"

echo "Running cygwrun test suite on: $phost"

$cygwrun
test $? -eq 111 && xbexit 1 "Failed #1.1"

tmpdir="`$_cygwrun . /tmp`"
test $? -ne 0 && xbexit 1 "Failed #1.2: \`$tmpdir'"
vardir="`$_cygwrun . /var`"
test $? -ne 0 && xbexit 1 "Failed #1.3"
usrdir="`$_cygwrun . /usr`"
test $? -ne 0 && xbexit 1 "Failed #1.4"


rv="`$_cygwrun $_dumpargs /opt=/tmp/foo`"
test "x$rv" = "x/opt=/tmp/foo" || xbexit 1 "Failed #3.1: \`$rv'"
rv="`$_cygwrun . /opt:/tmp/foo`"
test "x$rv" = "x/opt:/tmp/foo" || xbexit 1 "Failed #3.2: \`$rv'"
rv="`$_cygwrun . I=/tmp/foo`"
test "x$rv" = "xI=$tmpdir\\foo" || xbexit 1 "Failed #3.3: \`$rv'"
rv="`$_cygwrun . -I=/tmp/foo`"
test "x$rv" = "x-I=$tmpdir\\foo" || xbexit 1 "Failed #3.4: \`$rv'"
rv="`$_cygwrun . --I=/tmp/foo`"
test "x$rv" = "x--I=$tmpdir\\foo" || xbexit 1 "Failed #3.5: \`$rv'"
rv="`$_cygwrun . --I=/tmp`"
test "x$rv" = "x--I=$tmpdir" || xbexit 1 "Failed #3.6: \`$rv'"
rv="`$_cygwrun . I=/tmp/foo:/tmp/bar`"
test "x$rv" = "xI=$tmpdir\\foo;$tmpdir\\bar" || xbexit 1 "Failed #3.7: \`$rv'"
rv="`$_cygwrun . \"I=/tmp/foo::  :/tmp/bar\"`"
test "x$rv" = "xI=$tmpdir\\foo;$tmpdir\\bar" || xbexit 1 "Failed #3.8: \`$rv'"
rv="`$_cygwrun . I=\"\\\"/tmp/\"\\\"`"
test "x$rv" = "xI=\"$tmpdir\"" || xbexit 1 "Failed #3.9: \`$rv'"
rv="`$_cygwrun . I=\"\\"/tmp/\"`"
test "x$rv" = "xI=\"/tmp/" || xbexit 1 "Failed #3.10: \`$rv'"
rv="`$_cygwrun . I=\"\\\"./tmp\"\\\"`"
test "x$rv" = "xI=\".\\tmp\"" || xbexit 1 "Failed #3.11: \`$rv'"
rv="`$_cygwrun . I=\"\\\"./tmp:./bar\"\\\"`"
test "x$rv" = "xI=\".\\tmp;.\\bar\"" || xbexit 1 "Failed #3.12: \`$rv'"

rv="`$_cygwrun . ./tmp`"
test "x$rv" = "x.\\tmp" || xbexit 1 "Failed #4.1: \`$rv'"
rv="`$_cygwrun . ../tmp///foo`"
test "x$rv" = "x..\\tmp\\foo" || xbexit 1 "Failed #4.2: \`$rv'"
rv="`$_cygwrun . .../tmp`"
test "x$rv" = "x.../tmp" || xbexit 1 "Failed #4.3: \`$rv'"
rv="`$_cygwrun . \"\\\"./tmp\"\\\"`"
test "x$rv" = "x\"./tmp\"" || xbexit 1 "Failed #4.4: \`$rv'"
rv="`$_cygwrun . ..`"
test "x$rv" = "x.." || xbexit 1 "Failed #4.5: \`$rv'"
rv="`$_cygwrun . ./tmp/.//foo/`"
test "x$rv" = "x.\\tmp\\foo" || xbexit 1 "Failed #4.6: \`$rv'"

rv="`$_cygwrun $_dumpargs -f1=../tmp/foo`"
test "x$rv" = "x-f1=..\\tmp\\foo" || xbexit 1 "Failed #5.1: \`$rv'"
rv="`$_cygwrun $_dumpargs --f=../tmp/foo`"
test "x$rv" = "x--f=..\\tmp\\foo" || xbexit 1 "Failed #5.2: \`$rv'"
rv="`$_cygwrun $_dumpargs f=../tmp/foo`"
test "x$rv" = "xf=..\\tmp\\foo" || xbexit 1 "Failed #5.3: \`$rv'"
cd ./.build
rv="`$_cygwrun dumpargs 'f=../tmp/foo/;'`"
test "x$rv" = "xf=..\\tmp\\foo" || xbexit 1 "Failed #5.4: \`$rv'"
cd ..

export FOO="/tmp/a::/tmp/b:"
rv="`$_cygwrun $_dumpenvp FOO`"
test "x$rv" = "xFOO=$tmpdir\\a;$tmpdir\\b" || xbexit 1 "Failed #6.1: \`$rv'"
export FOO="/tmp:/var: :../d"
export BAR="/tmp/bin"
rv="`$_cygwrun $_dumpenvp FOO`"
test "x$rv" = "xFOO=$tmpdir;$vardir;..\\d" || xbexit 1 "Failed #6.2: \`$rv'"
export FOO="c*"
export CYGWRUN_SKIP=BAR
rv="`$_cygwrun $_dumpenvp BAR`"
test "x$rv" = "xBAR=/tmp/bin" || xbexit 1 "Failed #6.3: \`$rv'"
export CYGWRUN_SKIP=FOO,BAR
rv="`$_cygwrun $_dumpenvp FOO`"
test "x$rv" = "xFOO=c*" || xbexit 1 "Failed #6.4: \`$rv'"

echo "All tests passed!"
exit 0
