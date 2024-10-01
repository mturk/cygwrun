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
# --------------------------------------------------
# Cygwrun release helper script
#
# Usage: mkrelease.sh version [make arguments]
#    eg: mkrelease.sh 1.2.3
#        mkrelease.sh 1.2.3_1 _BUILD_VENDOR=_1.acme _BUILD_NUMBER=`date +%y%j`
#

eexit()
{
    e=$1; shift;
    echo "$@" 1>&2
    echo 1>&2
    echo "Usage: mkrelease.sh version [make arguments]" 1>&2
    echo "   eg: mkrelease.sh 1.2.3" 1>&2
    exit $e
}

case "`uname -o`" in
  Cygwin)
    BuildHost="cygwin"
    BuildCC="`x86_64-w64-mingw32-gcc --version | head -1`"
  ;;
  Msys)
    BuildHost="msys2"
    BuildCC="`gcc --version | head -1`"
  ;;
  *)
    echo "Unknown `uname`"
    echo "This script can run only inside cygwin"
    exit 1
  ;;

esac

ReleaseVersion=$1
test "x$ReleaseVersion" = "x" && eexit 1 "Missing version argument"
shift
BuildDir=.build
ProjectName=cygwrun
ReleaseArch=win-x64
ReleaseName=$ProjectName-$ReleaseVersion-$ReleaseArch
ReleaseLog=$ReleaseName.txt
#
#
MakefileFlags="_BUILD_TIMESTAMP=`date +%Y%m%d%H%M%S` $*"
make clean
make $MakefileFlags
#
pushd $BuildDir >/dev/null
echo "## Binary release v$ReleaseVersion" > $ReleaseLog
echo >> $ReleaseLog
echo '```no-highlight' >> $ReleaseLog
echo "Compiled on $BuildHost `uname -r`" >> $ReleaseLog
echo "make $MakefileFlags" >> $ReleaseLog
echo "using: $BuildCC" >> $ReleaseLog
echo >> $ReleaseLog
echo >> $ReleaseLog
#
zip -q9 $ReleaseName.zip $ProjectName.exe
echo "SHA256 hash of $ReleaseName.zip:" >> $ReleaseLog
sha256sum $ReleaseName.zip | sed 's;\ .*;;' >> $ReleaseLog
echo >> $ReleaseLog
echo '```' >> $ReleaseLog
#
popd >/dev/null

