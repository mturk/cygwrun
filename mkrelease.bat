@echo off
rem Licensed to the Apache Software Foundation (ASF) under one or more
rem contributor license agreements.  See the NOTICE file distributed with
rem this work for additional information regarding copyright ownership.
rem The ASF licenses this file to You under the Apache License, Version 2.0
rem (the "License"); you may not use this file except in compliance with
rem the License.  You may obtain a copy of the License at
rem
rem     http://www.apache.org/licenses/LICENSE-2.0
rem
rem Unless required by applicable law or agreed to in writing, software
rem distributed under the License is distributed on an "AS IS" BASIS,
rem WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
rem See the License for the specific language governing permissions and
rem limitations under the License.
rem
rem --------------------------------------------------
rem Cygwrun release helper script
rem
rem Usage: mkrelease.bat [options] version [make arguments]
rem    eg: mkrelease 1.2.3
rem        mkrelease 1.3.3_45 "_VENDOR_SFX=_1.acme" "_BUILD_NUMBER=45"
rem        mkrelease /s ...   compile with static msvcrt

rem
setlocal
rem
set "ProjectName=cygwrun"
set "BuildDir=x64"
set "ReleaseArch=win-x64"
rem
rem Get timestamp using Powershell
for /f "delims=" %%# in ('powershell get-date -format "{yyyyddMMHHmmss}"') do @set BuildTimestamp=%%#
set "MakefileArgs=_BUILD_TIMESTAMP=%BuildTimestamp%"
rem
if /i "x%~1" == "x/s" goto setStatic
rem
goto doneOpts
rem
:setStatic
set "MakefileArgs=%MakefileArgs% _STATIC_MSVCRT=1"
shift
rem
:doneOpts
rem
if "x%~1" == "x" goto Einval
rem
set "ReleaseVersion=%~1"
shift
rem
:setArgs
if "x%~1" == "x" goto doneArgs
set "MakefileArgs=%MakefileArgs% %~1"
shift
goto setArgs
rem
:doneArgs
rem
set "ReleaseName=%ProjectName%-%ReleaseVersion%-%ReleaseArch%"
set "ReleaseLog=%ReleaseName%.txt
rem
rem Create builds
nmake /nologo /f Makefile.mak clean
nmake /nologo /f Makefile.mak %MakefileArgs%
if not %ERRORLEVEL% == 0 goto Failed
rem
pushd "%BuildDir%"
rem
rem Get nmake and cl versions
rem
echo _MSC_FULL_VER > %ProjectName%.i
nmake /? 2>%ProjectName%.p 1>NUL
cl.exe /EP %ProjectName%.i >>%ProjectName%.p 2>&1
rem
echo ## Binary release v%ReleaseVersion% > %ReleaseLog%
echo. >> %ReleaseLog%
echo ```no-highlight >> %ReleaseLog%
echo Compiled using: >> %ReleaseLog%
echo nmake /f Makefile.mak %MakefileArgs% >> %ReleaseLog%
findstr /B /C:"Microsoft (R) " %ProjectName%.p >> %ReleaseLog%
echo. >> %ReleaseLog%
rem
del /F /Q %ProjectName%.i 2>NUL
del /F /Q %ProjectName%.p 2>NUL
echo. >> %ReleaseLog%
echo. >> %ReleaseLog%
7za.exe a -bd %ReleaseName%.zip %ProjectName%.exe
certutil -hashfile %ReleaseName%.zip SHA256 | findstr /v "CertUtil" >> %ReleaseLog%
echo. >> %ReleaseLog%
echo ``` >> %ReleaseLog%
popd
goto End
rem
:Einval
echo Error: Invalid parameter
echo Usage: %~nx0 [options] version [make arguments]
exit /b 1
rem
:Failed
echo.
echo Error: Cannot build %ProjectName%.exe
exit /b 1
rem
:End
exit /b 0
