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

echo "Running $phost test suite"

# Test for working directory
mkdir $srcdir/.test 2>/dev/null
cp -f $_cygwrun $srcdir/.test/cygwrun_test.exe
rv="`$_cygwrun -w $srcdir/.test cygwrun_test -e _CYGWRUN_POSIX_ROOT`"
rc=$?
rm -rf $srcdir/.test 2>/dev/null
test $rc -ne 0 && xbexit 1 "Failed #0"

export POSIX_ROOT="c:\\_not\\a/directory//"
v=/tmp
rv="`$_cygwrun -p $v`"
test ".$rv" = ".C:\\_not\\a\\directory\\tmp" || xbexit 1 "Failed #1: \`$rv'"
export POSIX_ROOT=

export FOO=bar
rv="`$_cygwrun -e FOO`"
test ".$rv" = ".FOO=bar" || xbexit 1 "Failed #2: \`$rv'"

export FOO=/Fo:/tmp:/usr
rv="`$_cygwrun -e FOO`"
test ".$rv" = ".FOO=/Fo:/tmp:/usr" || xbexit 1 "Failed #3: \`$rv'"

tmpdir="`$_cygwrun -p /tmp`"
test $? -ne 0 && xbexit 1 "Failed #4"

usrdir="`$_cygwrun -p /usr`"
test $? -ne 0 && xbexit 1 "Failed #5"

export FOO="/tmp/foo/bar:/usr/local"
rv="`$_cygwrun -e FOO`"
test ".$rv" = ".FOO=$tmpdir\\foo\\bar;$usrdir\\local" || xbexit 1 "Failed #6: \`$rv'"

export FOO="-unknown:/tmp/foo/bar:/usr/local"
rv="`$_cygwrun -e FOO`"
test ".$rv" = ".FOO=$FOO" || xbexit 1 "Failed #7: \`$rv'"
export FOO="/tmp/foo/bar::unknown:/usr/local"
rv="`$_cygwrun -e FOO`"
test ".$rv" = ".FOO=$FOO" || xbexit 1 "Failed #7.1: \`$rv'"
rv="`$_cygwrun -p -I=/tmp/foo`"
test ".$rv" = ".-I=$tmpdir\\foo" || xbexit 1 "Failed #7.2: \`$rv'"

export FOO="/usr/a::/usr/b"
rv="`$_cygwrun -e FOO`"
test ".$rv" = ".FOO=$usrdir\\a;$usrdir\\b" || xbexit 1 "Failed #8: \`$rv'"

export FOO="/usr/a:$abcd0123456789::/usr/b"
rv="`$_cygwrun -e FOO`"
test ".$rv" = ".FOO=$usrdir\\a;$usrdir\\b" || xbexit 1 "Failed #8.1: \`$rv'"

rv="`$_cygwrun -p /libpath:./tmp/foo`"
test ".$rv" = "./libpath:.\\tmp\\foo" || xbexit 1 "Failed #9: \`$rv'"

rv="`$_cygwrun -p ./tmp`"
test ".$rv" = "..\\tmp" || xbexit 1 "Failed #10.1: \`$rv'"

rv="`$_cygwrun -p ../tmp/foo`"
test ".$rv" = "...\\tmp\\foo" || xbexit 1 "Failed #10.2: \`$rv'"

rv="`$_cygwrun -p ../tmp:/foo`"
test ".$rv" = ".../tmp:/foo" || xbexit 1 "Failed #10.3: \`$rv'"

echo "All tests passed!"
exit 0
