default: berntcpt
CC = gcc
CFLAGS += -Wall -Wextra -march=native -Ofast -pipe -I../include -flto -fuse-linker-plugin -MMD
LDFLAGS += -march=native -Ofast -pipe -flto -fuse-linker-plugin

#default: berntcpt.exe
#CC = x86_64-w64-mingw32-gcc
#STRIP = x86_64-w64-mingw32-strip
#CFLAGS += -DWINVER=0x0600 -D_WIN32_WINNT=0x0600 -mconsole -Wall -Wextra -Ofast -pipe
#LDFLAGS += -mconsole -Ofast -pipe -lqwave -lws2_32 -lmswsock
#######################
##CFLAGS += -DWINVER=0x0502 -D_WIN32_WINNT=0x0502 -mconsole -Wall -Wextra -Ofast -pipe
##LDFLAGS += -mconsole -Ofast -pipe -lqwave -lws2_32 -lmswsock -lntdll

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
