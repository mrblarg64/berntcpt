default: linux

VERSION = 1.6

SRCS = $(wildcard src/*.c)

CC = gcc

OBJS.linux = $(patsubst src/%.c, obj/%.o, $(SRCS))
CFLAGS.linux = -Wall -Wextra -march=native -Ofast -pipe -flto -fuse-linker-plugin -MMD
LDFLAGS.linux = -march=native -Ofast -pipe -flto -fuse-linker-plugin

BINPREFIX.32bit = i686-w64-mingw32-
BINPREFIX.64bit = x86_64-w64-mingw32-
STRIP = strip
CFLAGS.win =  -mconsole -Wall -Wextra -Ofast -pipe
LDFLAGS.win = -mconsole -Ofast -pipe -lws2_32 -lmswsock
#Windows 10 1703 newer
CFLAGS.10-1703-newer = -DNTDDI_VERSION=NTDDI_WIN10_RS2 -DWINVER=0x0A00 -D_WIN32_WINNT=0x0A00
LDFLAGS.10-1703-newer = -lqwave
########################
#Windows Vista newer
CFLAGS.vista-newer = -DWINVER=0x0600 -D_WIN32_WINNT=0x0600
LDFLAGS.vista-newer = -lqwave
########################
#Windows Server 2003 newer
CFLAGS.server-2003-newer += -DWINVER=0x0502 -D_WIN32_WINNT=0x0502
LDFLAGS.server-2003-newer = 
########################
#NT 4 - 95, NT 3?
CFLAGS.nt-4-newer = -DWINVER=0x0400 -D_WIN32_WINNT=0x0400
LDFLAGS.nt-4-newer = 

WINDOWS_VERSIONS = 10-1703-newer vista-newer server-2003-newer # nt-4-newer
WINDOWS_BITS = 32bit 64bit

#DEPENDS = $(SRCS:.c=.d)

.PHONY: all
.PHONY: windows
.PHONY: linux

all: linux windows

linux: berntcpt
windows: $(foreach bits, $(WINDOWS_BITS), $(foreach winver, $(WINDOWS_VERSIONS), berntcpt-$(VERSION)-$(winver)-$(bits).exe))

define winexe =
OBJS.$$$(1)-$$$(2) = $(patsubst src/%.c, obj-$$$(1)-$$$(2)/%.o, $(SRCS))

obj-$$$(1)-$$$(2)/%.o: src/%.c | obj-$$$(1)-$$$(2)
	$$(BINPREFIX.$$$(2))$(CC) $$(CFLAGS.win) $$(CFLAGS.$$$(1)) -c -o $$@ $$<

obj-$$$(1)-$$$(2):
	mkdir $$@

berntcpt-$$(VERSION)-$$$(1)-$$$(2).exe: $$(OBJS.$$$(1)-$$$(2))
	$$(BINPREFIX.$$$(2))$(CC) $$(OBJS.$$$(1)-$$$(2)) -o $$@ $$(LDFLAGS.win) $$(LDFLAGS.$$$(1))
	$$(BINPREFIX.$$$(2))$$(STRIP) $$@
endef

$(foreach bits, $(WINDOWS_BITS), $(foreach winver, $(WINDOWS_VERSIONS), $(eval $(call winexe, $(winver), $(bits)))))


berntcpt: $(OBJS.linux)
	$(CC) $(OBJS.linux) -o berntcpt $(LDFLAGS.linux)

obj/%.o: src/%.c | obj
	$(CC) $(CFLAGS.linux) -c -o $@ $<

obj:
	mkdir $@

#-include $(DEPENDS)

clean:
	$(RM) berntcpt *.exe obj*/*
	rmdir obj*
