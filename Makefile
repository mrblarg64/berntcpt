default: berntcpt
CC = gcc
CFLAGS += -Wall -Wextra -march=native -Ofast -pipe -I../include -flto -fuse-linker-plugin -MMD
LDFLAGS += -march=native -Ofast -pipe -flto -fuse-linker-plugin

#default: berntcpt.exe
#BINPREFIX = i686-w64-mingw32-
#BINPREFIX = x86_64-w64-mingw32-
#CC = $(BINPREFIX)gcc
#STRIP = $(BINPREFIX)strip
#Windows 10 19H1 newer
#CFLAGS += -DNTDDI_VERSION=NTDDI_WIN10_19H1 -DWINVER=0x0A00 -D_WIN32_WINNT=0x0A00 -mconsole -Wall -Wextra -Ofast -pipe
#LDFLAGS += -mconsole -Ofast -pipe -lqwave -lws2_32 -lmswsock
########################
#Windows Vista newer
#CFLAGS += -DWINVER=0x0600 -D_WIN32_WINNT=0x0600 -mconsole -Wall -Wextra -Ofast -pipe
#LDFLAGS += -mconsole -Ofast -pipe -lqwave -lws2_32 -lmswsock
########################
#Windows Server 2003 newer
#CFLAGS += -DWINVER=0x0502 -D_WIN32_WINNT=0x0502 -mconsole -Wall -Wextra -Ofast -pipe
#LDFLAGS += -mconsole -Ofast -pipe -lws2_32 -lmswsock
########################
#NT 4 - 95, NT 3?
#CFLAGS += -DWINVER=0x0400 -D_WIN32_WINNT=0x0400 -mconsole -Wall -Wextra -Ofast -pipe
#LDFLAGS += -mconsole -Ofast -pipe -lws2_32 -lmswsock

SRCS = $(wildcard src/*.c)
OBJS = $(SRCS:.c=.o)
#DEPENDS = $(SRCS:.c=.d)



windows: berntcpt.exe
linux: berntcpt

berntcpt.exe: $(OBJS)
	$(CC) $(OBJS) -o berntcpt $(LDFLAGS)
	$(STRIP) berntcpt.exe

berntcpt: $(OBJS)
	$(CC) $(OBJS) -o berntcpt $(LDFLAGS)

#-include $(DEPENDS)

clean:
	$(RM) berntcpt berntcpt.exe src/*.o src/*.d
