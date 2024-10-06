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
# Compile native Windows binary using Visual Studio
#
#
CC = cl.exe
LN = link.exe
RC = rc.exe

MACHINE = x64
SRCDIR  = .
PROJECT = cygwrun
WINVER  = 0x0A00

!IF DEFINED(_MSVC_DLLCRT)
CRT_CFLAGS = -MD
!ELSE
CRT_CFLAGS = -MT
EXTRA_LFLAGS = $(EXTRA_LFLAGS) /NODEFAULTLIB:libucrt.lib /DEFAULTLIB:ucrt.lib
!ENDIF

WORKDIR = $(SRCDIR)\.build
OUTPUT  = $(WORKDIR)\cygwrun.exe
TESTDA  = $(WORKDIR)\dumpargs.exe
TESTDE  = $(WORKDIR)\dumpenvp.exe

CFLAGS = -DNDEBUG -D_WIN32_WINNT=$(WINVER) -DWINVER=$(WINVER) -DWIN32_LEAN_AND_MEAN
CFLAGS = $(CFLAGS) -D_CRT_SECURE_NO_WARNINGS  -D_CRT_SECURE_NO_DEPRECATE $(EXTRA_CFLAGS)
CLOPTS = -c -nologo $(CRT_CFLAGS) -W4 -O2 -Ob2 -GF -Gs0
RCOPTS = -nologo -l 0x409 -n
RFLAGS = -d NDEBUG -d WINVER=$(WINVER) -d _WIN32_WINNT=$(WINVER) -d WIN32_LEAN_AND_MEAN $(EXTRA_RFLAGS)
LFLAGS = -nologo /INCREMENTAL:NO /OPT:REF /SUBSYSTEM:CONSOLE /MACHINE:$(MACHINE) $(EXTRA_LFLAGS)
LDLIBS = kernel32.lib advapi32.lib

!IF DEFINED(_BUILD_VENDOR)
CFLAGS = $(CFLAGS) -D_BUILD_VENDOR=$(_BUILD_VENDOR)
RFLAGS = $(RFLAGS) /d _BUILD_VENDOR=$(_BUILD_VENDOR)
!ENDIF
!IF DEFINED(_BUILD_NUMBER)
CFLAGS = $(CFLAGS) -D_BUILD_NUMBER=$(_BUILD_NUMBER)
RFLAGS = $(RFLAGS) /d _BUILD_NUMBER=$(_BUILD_NUMBER)
!ENDIF

OBJECTS = \
	$(WORKDIR)\cygwrun.obj \
	$(WORKDIR)\cygwrun.res

TESTDA_OBJECTS = \
	$(WORKDIR)\dumpargs.obj

TESTDE_OBJECTS = \
	$(WORKDIR)\dumpenvp.obj


all : $(WORKDIR) $(OUTPUT)

$(WORKDIR):
	@-md $(WORKDIR) 2>NUL

{$(SRCDIR)}.c{$(WORKDIR)}.obj:
	$(CC) $(CLOPTS) $(CFLAGS) -Fo$(WORKDIR)\ $<

{$(SRCDIR)\test}.c{$(WORKDIR)}.obj:
	$(CC) $(CLOPTS) $(CFLAGS) -Fo$(WORKDIR)\ $<

{$(SRCDIR)}.rc{$(WORKDIR)}.res:
	$(RC) $(RCOPTS) $(RFLAGS) -fo $@ $<

$(OUTPUT): $(WORKDIR) $(OBJECTS)
	$(LN) $(LFLAGS) $(OBJECTS) setargv.obj $(LDLIBS) -out:$@

$(TESTDA): $(WORKDIR) $(TESTDA_OBJECTS)
	$(LN) $(LFLAGS) $(TESTDA_OBJECTS) $(LDLIBS) -out:$@

$(TESTDE): $(WORKDIR) $(TESTDE_OBJECTS)
	$(LN) $(LFLAGS) $(TESTDE_OBJECTS) $(LDLIBS) -out:$@

test: all $(TESTDA) $(TESTDE)

clean:
	@-rd /S /Q $(WORKDIR) 2>NUL
