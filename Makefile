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
# Compile native Windows binary using mingw64
# make -f Makefile.gmk
#
# To compile from Cygwin environment
# make -f Makefile.gmk USE_MINGW_PACKAGE_PREFIX=1
#
# the following packages need to be installed:
#
# mingw64-x86_64-binutils,
# mingw64-x86_64-gcc-core,
# mingw64-x86_64-headers,
# mingw64-x86_64-runtime
#

ifdef USE_MINGW_PACKAGE_PREFIX
CC = x86_64-w64-mingw32-gcc
RC = x86_64-w64-mingw32-windres
RL = x86_64-w64-mingw32-strip
else
CC = gcc
RC = windres
RL = strip
endif
LN = $(CC)

SRCDIR  = .
PROJECT = cygwrun
TARGET  = $(PROJECT).exe
WORKDIR = $(SRCDIR)/x64
OUTPUT  = $(WORKDIR)/$(TARGET)
TESTDA  = $(WORKDIR)/dumpargs.exe
TESTDE  = $(WORKDIR)/dumpenvp.exe

WINVER  = 0x0601
CFLAGS  = -DNDEBUG -D_WIN32_WINNT=$(WINVER) -DWINVER=$(WINVER) -DWIN32_LEAN_AND_MEAN \
	-D_CRT_SECURE_NO_WARNINGS -D_CRT_SECURE_NO_DEPRECATE \
	-DUNICODE -D_UNICODE $(EXTRA_CFLAGS)
LNOPTS  = -m64 -O2 -Wall -Wextra -Wno-unused-parameter -Wno-unused-function -Wno-incompatible-pointer-types -municode -mconsole
RCOPTS  = -l 0x409 -F pe-x86-64 -O coff
RFLAGS  = -D NDEBUG -D _WIN32_WINNT=$(WINVER) -D WINVER=$(WINVER) $(EXTRA_RFLAGS)
CLOPTS  = $(LNOPTS) -c
LDLIBS  = -lkernel32
RLOPTS  = --strip-unneeded

ifdef _VENDOR_SFX
CFLAGS += -D_VENDOR_SFX=$(_VENDOR_SFX)
RFLAGS += -D _VENDOR_SFX=$(_VENDOR_SFX)
endif
ifdef _VENDOR_NUM
CFLAGS += -D_VENDOR_NUM=$(_VENDOR_NUM)
RFLAGS += -D _VENDOR_NUM=$(_VENDOR_NUM)
endif
ifdef _BUILD_TIMESTAMP
CFLAGS += -D_BUILD_TIMESTAMP=$(_BUILD_TIMESTAMP)
RFLAGS += -D _BUILD_TIMESTAMP=$(_BUILD_TIMESTAMP)
endif

OBJECTS = \
	$(WORKDIR)/$(PROJECT).o \
	$(WORKDIR)/$(PROJECT).res

TESTDA_OBJECTS = \
	$(WORKDIR)/dumpargs.o

TESTDE_OBJECTS = \
	$(WORKDIR)/dumpenvp.o

all : $(WORKDIR) $(OUTPUT)
	@:

$(WORKDIR):
	@mkdir -p $@

$(WORKDIR)/%.o: $(SRCDIR)/%.c $(SRCDIR)/%.h
	$(CC) $(CLOPTS) -o $@ $(CFLAGS) -I$(SRCDIR) $<

$(WORKDIR)/%.o: $(SRCDIR)/test/%.c
	$(CC) $(CLOPTS) -o $@ $(CFLAGS) -I$(SRCDIR) $<

$(WORKDIR)/%.res: $(SRCDIR)/%.rc $(SRCDIR)/%.h
	$(RC) $(RCOPTS) -o $@ $(RFLAGS) -I $(SRCDIR) $<

$(OUTPUT): $(WORKDIR) $(OBJECTS)
	$(LN) $(LNOPTS) -o $@ $(OBJECTS) $(LDLIBS)
	$(RL) $(RLOPTS) $@

$(TESTDA): $(WORKDIR) $(TESTDA_OBJECTS)
	$(LN) $(LNOPTS) -o $@ $(TESTDA_OBJECTS) $(LDLIBS)

$(TESTDE): $(WORKDIR) $(TESTDE_OBJECTS)
	$(LN) $(LNOPTS) -o $@ $(TESTDE_OBJECTS) $(LDLIBS)

test: all $(TESTDA) $(TESTDE)
	@echo
	@echo Running simple verification tests ...
	@echo
	@$(OUTPUT) $(TESTDE) SHELL
	@echo
	@$(OUTPUT) $(TESTDA) //tmp
	@echo
	@echo
	@echo You can now run runtest.sh from Cygwin environment

clean:
	@rm -rf $(WORKDIR)

.PHONY: all clean
