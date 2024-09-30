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
# To compile from the Cygwin environment
# the following packages need to be installed:
#
# mingw64-x86_64-binutils,
# mingw64-x86_64-gcc-core,
# mingw64-x86_64-headers,
# mingw64-x86_64-runtime
#

CC = x86_64-w64-mingw32-gcc
RC = x86_64-w64-mingw32-windres
RL = x86_64-w64-mingw32-strip
LN = $(CC)

SRCDIR  = .
WORKDIR = $(SRCDIR)/.build
OUTPUT  = $(WORKDIR)/cygwrun.exe
TESTDA  = $(WORKDIR)/dumpargs.exe
TESTDE  = $(WORKDIR)/dumpenvp.exe

WINVER  = 0x0A00
CFLAGS  = -DNDEBUG -D_WIN32_WINNT=$(WINVER) -DWINVER=$(WINVER) -DWIN32_LEAN_AND_MEAN $(EXTRA_CFLAGS)
LNOPTS  = -m64 -O2 -Wall -Wextra -Wno-unused-parameter -Wno-unused-function -Wno-incompatible-pointer-types -mconsole
RCOPTS  = -l 0x409 -F pe-x86-64 -O coff
RFLAGS  = -D NDEBUG -D _WIN32_WINNT=$(WINVER) -D WINVER=$(WINVER)
CLOPTS  = $(LNOPTS) -c
LDLIBS  = -lkernel32
RLOPTS  = --strip-unneeded

ifdef _BUILD_VENDOR
CFLAGS += -D_BUILD_VENDOR=$(_BUILD_VENDOR)
RFLAGS += -D _BUILD_VENDOR=$(_BUILD_VENDOR)
endif
ifdef _BUILD_NUMBER
CFLAGS += -D_BUILD_NUMBER=$(_BUILD_NUMBER)
RFLAGS += -D _BUILD_NUMBER=$(_BUILD_NUMBER)
endif
ifdef _BUILD_TIMESTAMP
CFLAGS += -D_BUILD_TIMESTAMP=$(_BUILD_TIMESTAMP)
RFLAGS += -D _BUILD_TIMESTAMP=$(_BUILD_TIMESTAMP)
endif

OBJECTS = \
	$(WORKDIR)/cygwrun.o \
	$(WORKDIR)/cygwrun.res

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
	$(RL) $(RLOPTS) $@

$(TESTDE): $(WORKDIR) $(TESTDE_OBJECTS)
	$(LN) $(LNOPTS) -o $@ $(TESTDE_OBJECTS) $(LDLIBS)
	$(RL) $(RLOPTS) $@

test: all $(TESTDA) $(TESTDE)
	@echo
	@$(SRCDIR)/runtest.sh
	@echo

clean:
	@rm -rf $(WORKDIR)

.PHONY: all clean
