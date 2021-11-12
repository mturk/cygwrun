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
p=`basename $0`
r=`dirname $0`
srcdir="`cd \"$r\" && pwd`"

case "`uname`" in
  CYGWIN*)
    phost=cygwin
  ;;
  MINGW*|MSYS*)
    phost=mingw
  ;;
  *)
    echo "Unknown `uname`"
    echo "Test suite can run only inside cygwin or mingw"
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

echo "Running test suite inside $phost"

# Test for working directory
mkdir $srcdir/.test 2>/dev/null
cp -f $_cygwrun $srcdir/.test/cygwrun_test.exe
rv=`$_cygwrun -w $srcdir/.test cygwrun_test -e POSIX_ROOT=`
test $? -ne 0 && xbexit 1 "Failed #1"
rm -rf $srcdir/.test 2>/dev/null

export FOO=bar
rv=`$_cygwrun -e FOO=`
test ".$rv" = ".FOO=bar" || xbexit 1 "Failed #2: \`$rv'"

if [ $phost = cygwin ]
then
  export FOO=/Fo:/tmp:/usr
  rv=`$_cygwrun -e FOO=`
  test ".$rv" = ".FOO=/Fo:/tmp:/usr" || xbexit 1 "Failed #3: \`$rv'"
fi

echo "All tests passed!"
exit 0