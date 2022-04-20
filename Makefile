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
# Originally contributed by Mladen Turk <mturk apache.org>
#
CC = cl.exe
LN = link.exe
RC = rc.exe

MACHINE = x64
SRCDIR  = .
PROJECT = cygwrun
WINVER  = 0x0601
EXEVER  = 1.1

!IF DEFINED(_STATIC_MSVCRT)
CRT_CFLAGS = -MT
EXTRA_LIBS =
!ELSE
CRT_CFLAGS = -MD
!ENDIF

WORKDIR = $(SRCDIR)\$(MACHINE)
OUTPUT  = $(WORKDIR)\$(PROJECT).exe
TESTDA  = $(WORKDIR)\dumpargs.exe
TESTDE  = $(WORKDIR)\dumpenvp.exe

CFLAGS = -DNDEBUG -D_WIN32_WINNT=$(WINVER) -DWINVER=$(WINVER) -DWIN32_LEAN_AND_MEAN
CFLAGS = $(CFLAGS) -D_CRT_SECURE_NO_WARNINGS  -D_CRT_SECURE_NO_DEPRECATE
CFLAGS = $(CFLAGS) -DUNICODE -D_UNICODE $(DFLAGS) $(EXTRA_CFLAGS)
CLOPTS = /c /nologo $(CRT_CFLAGS) -W4 -O2 -Ob2
RCOPTS = /l 0x409 /n
RFLAGS = /d NDEBUG /d WINVER=$(WINVER) /d _WIN32_WINNT=$(WINVER) /d WIN32_LEAN_AND_MEAN $(EXTRA_RFLAGS)
LFLAGS = /nologo /INCREMENTAL:NO /OPT:REF /SUBSYSTEM:CONSOLE /MACHINE:$(MACHINE) /VERSION:$(EXEVER) $(EXTRA_LFLAGS)
LDLIBS = kernel32.lib $(EXTRA_LIBS)

!IF DEFINED(VSCMD_VER)
RCOPTS = /nologo $(RCOPTS)
!ENDIF

!IF DEFINED(_VENDOR_SFX)
CFLAGS = $(CFLAGS) -D_VENDOR_SFX=$(_VENDOR_SFX)
RFLAGS = $(RFLAGS) /d _VENDOR_SFX=$(_VENDOR_SFX)
!ENDIF
!IF DEFINED(_VENDOR_NUM)
RFLAGS = $(RFLAGS) /d _VENDOR_NUM=$(_VENDOR_NUM)
!ENDIF

OBJECTS = \
	$(WORKDIR)\$(PROJECT).obj \
	$(WORKDIR)\$(PROJECT).res

TESTDA_OBJECTS = \
	$(WORKDIR)\dumpargs.obj

TESTDE_OBJECTS = \
	$(WORKDIR)\dumpenvp.obj


all : $(WORKDIR) $(OUTPUT)

$(WORKDIR):
	@-md $(WORKDIR)

{$(SRCDIR)}.c{$(WORKDIR)}.obj:
	$(CC) $(CLOPTS) $(CFLAGS) -Fo$(WORKDIR)\ $<

{$(SRCDIR)\test}.c{$(WORKDIR)}.obj:
	$(CC) $(CLOPTS) $(CFLAGS) -Fo$(WORKDIR)\ $<

{$(SRCDIR)}.rc{$(WORKDIR)}.res:
	$(RC) $(RCOPTS) $(RFLAGS) /fo $@ $<

$(OUTPUT): $(WORKDIR) $(OBJECTS)
	$(LN) $(LFLAGS) $(OBJECTS) $(LDLIBS) /out:$@

$(TESTDA): $(WORKDIR) $(TESTDA_OBJECTS)
	$(LN) $(LFLAGS) $(TESTDA_OBJECTS) $(LDLIBS) /out:$@

$(TESTDE): $(WORKDIR) $(TESTDE_OBJECTS)
	$(LN) $(LFLAGS) $(TESTDE_OBJECTS) $(LDLIBS) /out:$@

test: all $(TESTDA) $(TESTDE)
	@echo.
	@echo Running simple verification tests ...
	@echo.
	@$(OUTPUT) $(TESTDE) MAKEDIR
	@echo.
	@$(OUTPUT) $(TESTDA) /Foo:/tmp
	@echo.
	@echo.
	@echo You can now call runtest.sh from Cygwin environment

clean:
	@-rd /S /Q $(WORKDIR) 2>NUL
