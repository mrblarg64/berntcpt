//Copyright (C) 2026 Brian William Denton
//Available under the GNU GPLv3 License

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
//#include <winsock2.h>
#include <mswsock.h>
#include <ws2tcpip.h>
#if (WINVER >= 0x0600)
#include <qos2.h>
#else
#include <mstcpip.h>
#include <ntdef.h>
#include <ntstatus.h>
#endif

#define ERRNO_T DWORD
#define SOCKET_T SOCKET
#define NEWLINE "\r\n"
#define U64_PF "%llu"
#define SSO_CAST (const char*)
#define B_SHUT_RD SD_RECEIVE
#define B_SHUT_WR SD_SEND

#if (WINVER >= 0x0600)
QOS_FLOWID qosfid = 0;
#endif

HANDLE stdouth;
DWORD bout;
#define FASTPRINT(str) (WriteFile(stdouth, str, sizeof(str) - 1, &bout, NULL))

#define SOCKERROR(str) do {			\
wsaerrno = WSAGetLastError();	\
printf(str " 0x%x" NEWLINE, wsaerrno);	\
ExitProcess(wsaerrno);\
} while (0)

#define SEND_MSG_MORE(inbuf, size) do {\
		wsab.len = size;\
		wsab.buf = (char *)inbuf;				\
		if (WSASend(sock, &wsab, 1, &sentbytes, MSG_PARTIAL, NULL, NULL))\
		{\
			wsaerrno = WSAGetLastError();\
			if (wsaerrno != WSA_IO_PENDING)\
			{\
				printf("WSASend() 0x%x" NEWLINE, wsaerrno);	\
				ExitProcess(wsaerrno);			\
			}						\
		}							\
	} while (0)

#define CLOSESOCK(fd) (closesocket(fd))

#define FASTEXIT(code) (ExitProcess(code))

#define CLOSEFILE(fd) (CloseHandle(fd))

#define BERNTRNSFR_LINUX_SENDFILE_MAX 2147483646

HANDLE fd;
WSADATA wsd;
#else

#define _GNU_SOURCE

#include <fcntl.h>
#include <linux/sched.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/sendfile.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <signal.h>
#include <unistd.h>
#include <locale.h>
#include <sched.h>
#include <errno.h>

#define ERRNO_T int
#define SOCKET_T int
#define NEWLINE "\n"
#define U64_PF "%'lu"
#define SSO_CAST (void*)
#define B_SHUT_RD SHUT_RD
#define B_SHUT_WR SHUT_WR

#define FASTPRINT(str) ((void)!write(STDOUT_FILENO, str, sizeof(str) - 1))

#define SOCKERROR(str) do {			\
myerrno = errno;\
perror(str);\
_exit(myerrno);\
} while (0)

#define SEND_MSG_MORE(buf, size) do {\
if (send(sock, buf, size, MSG_MORE ) != sizeof(uint64_t))\
{\
	SOCKERROR("send()");\
}\
} while (0)

#define CLOSESOCK(fd) (close(fd))

#define FASTEXIT(code) (_exit(code))

#define CLOSEFILE(fd) (close(fd))

int fd;

#define BERNTRNSFR_LINUX_SENDFILE_MAX 0x7ffff000

const struct sigaction siga = {.sa_handler = SIG_IGN};

#endif

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#define BUFFER_SIZE 40960000

#define FLAG_TCP_SERVER 0b1
#define FLAG_SENDER 0b100
#define FLAG_PIPE 0b1000

#ifdef __ORDER_LITTLE_ENDIAN__
#define PIPE_STR 0x002d
#else
#define PIPE_STR 0x2d00
#endif


unsigned char state = 0;
uint32_t ip;
uint16_t port;
uint64_t size;
SOCKET_T sock;
#ifndef _WIN32
int pfd;
int pidfd;
pid_t child;
#endif

static inline void printusage()
{
        FASTPRINT("USAGE:" NEWLINE "\tberntcpt AC IPV4_PEER_ADDRESS PORT {FILE|-} [cmd [args [...]]]" NEWLINE NEWLINE "\tAC" NEWLINE "\t\tA = tcp socket \"s\"erver or \"c\"lient" NEWLINE "\t\tC = \"s\"end or \"r\"ecieve" NEWLINE NEWLINE "\tIf - is given as a file name, it will execute cmd and read it as a pipe IT WILL NOT READ FROM STDIN LIKE MANY OTHER PROGRAMS RUN WITH \"-\"!" NEWLINE);
}

static inline void exitbadusage()
{
	#ifdef _WIN32
	ExitProcess(ERROR_INVALID_PARAMETER);
	#else
	_exit(EINVAL);
	#endif
}

static inline void setupsockstorage(struct sockaddr_storage *sas, uint32_t i, uint16_t p)
{
	((struct sockaddr_in*)sas)->sin_family = AF_INET;
	((struct sockaddr_in*)sas)->sin_addr.s_addr = i;
	#ifdef __ORDER_LITTLE_ENDIAN__
	((struct sockaddr_in*)sas)->sin_port = __builtin_bswap16(p);
	#else
	((struct sockaddr_in*)sas)->sin_port = p;
	#endif

	return;
}

static inline void setupsocket()
{
	SOCKET_T s;
	ERRNO_T myerrno;
	struct sockaddr_storage listener = {0};
	struct sockaddr_storage peer = {0};
	socklen_t ssize;
	int sdhow;
	#ifdef _WIN32
	int wsaerrno;
	DWORD ssopt;
	#if (WINVER >= 0x0600)
	QOS_VERSION qosv;
	HANDLE qosh;
	#endif
	#else
	int ssopt;
	#endif

	if (state & FLAG_SENDER)
	{
		sdhow = B_SHUT_RD;
	}
	else
	{
		sdhow = B_SHUT_WR;
	}

	#ifdef _WIN32
	myerrno = WSAStartup(MAKEWORD(2, 2), &wsd);
	if (myerrno)
	{
		printf("failed WSAStartup() ecode 0x%lx" NEWLINE, myerrno);
		ExitProcess(myerrno);
	}

	

	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s == INVALID_SOCKET)
	{
		SOCKERROR("socket()");
	}
	#else


	s = socket(AF_INET, SOCK_CLOEXEC | SOCK_STREAM, 0);
	if (s == -1)
	{
		SOCKERROR("socket()");
	}
	#endif

	#ifdef _WIN32
	#if (WINVER >= 0x0600)
        qosv.MajorVersion = 1;
        qosv.MinorVersion = 0;
	if (!QOSCreateHandle(&qosv, &qosh))
	{
		myerrno = GetLastError();
		printf("QOSCreateHandle() failed 0x%lx" NEWLINE, myerrno);
	        FASTEXIT(myerrno);
	}
	#else
	//ssopt = IPTOS_DSCP_LE;
	ssopt = 0x04;
	if (setsockopt(s, IPPROTO_IP, IP_TOS, SSO_CAST &ssopt, sizeof(int)) == -1)
	{
	        SOCKERROR("IP_TOS (DSCP) setsockopt() failed");
	}
	#endif
	#else
	//on linux-6.5.5. these are inhereted after accept()
	//also the kernel's broken rt_tos2priority() function will
	//be fine with the IPTOS_DSCP_LE so there is no need
	//to setsockopt(SO_PRIORITY) (btw priority IS NOT inhereted)
	//I plan on having the default be IPTOS_DSCP_LE
	//bug me if you actually plan to use this server and don't like
	//that behaviour
	ssopt = IPTOS_DSCP_LE;
	if (setsockopt(s, IPPROTO_IP, IP_TOS, SSO_CAST &ssopt, sizeof(int)) == -1)
	{
	        SOCKERROR("IP_TOS (DSCP) setsockopt() failed");
	}
	#endif
	if (state & FLAG_TCP_SERVER)
	{
		ssopt = 1;
		if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, SSO_CAST &ssopt, sizeof(int)) == -1)
		{
		        SOCKERROR("SO_REUSEADDR setsockopt() failed");
		}
	}

	if (state & FLAG_TCP_SERVER)
	{
		setupsockstorage(&listener, 0, port);
		if (bind(s, (struct sockaddr*) &listener, sizeof(struct sockaddr_storage)) == -1)
		{
		        SOCKERROR("bind()");
		}

		if (listen(s, 1024) == -1)
		{
		        SOCKERROR("listen()");
		}

		while (1)
		{
			ssize = sizeof(struct sockaddr_storage);
			#ifdef _WIN32
			sock = accept(s, (struct sockaddr*) &peer, &ssize);
			if (sock == INVALID_SOCKET)
			{
				wsaerrno = WSAGetLastError();
				if (wsaerrno == WSAECONNABORTED)
				{
					continue;
				}
			        printf("accept() 0x%x" NEWLINE, wsaerrno);
				ExitProcess(wsaerrno);
			}
			#else
			sock = accept4(s, (struct sockaddr*) &peer, &ssize, SOCK_CLOEXEC);
			if (sock == -1)
			{
				if (errno == ECONNABORTED)
				{
					continue;
				}
				myerrno = errno;
				perror("accept()");
				_exit(myerrno);
			}
			#endif
			if (((struct sockaddr_in*)&peer)->sin_addr.s_addr == ip)
			{
				//Comment CLOSESOCK() out if you are running on
				//linux in wsl2 and mirrored networking mode
			        CLOSESOCK(s);
				#if ((defined(_WIN32)) && (WINVER >= 0x0600))
				if (!QOSAddSocketToFlow(qosh, sock, NULL, QOSTrafficTypeBackground, QOS_NON_ADAPTIVE_FLOW, &qosfid))
				{
					SOCKERROR("QOSAddSocketToFlow()");
				}
				#endif
				if (shutdown(sock, sdhow))
				{
					SOCKERROR("shutdown()");
				}
				return;
			}
		        FASTPRINT("got connection from non-peer!" NEWLINE);
		        CLOSESOCK(sock);
		}
	}
	else
	{
		sock = s;
		setupsockstorage(&peer, ip, port);
		if (connect(sock, (struct sockaddr*) &peer, sizeof(struct sockaddr_storage)))
		{
		        SOCKERROR("connect()");
		}
		#if ((defined(_WIN32)) && (WINVER >= 0x0600))
		if (!QOSAddSocketToFlow(qosh, sock, NULL, QOSTrafficTypeBackground, QOS_NON_ADAPTIVE_FLOW, &qosfid))
		{
			SOCKERROR("QOSAddSocketToFlow()");
		}
		#endif
		if (shutdown(sock, sdhow))
		{
			SOCKERROR("shutdown()");
		}
		return;
	}

	__builtin_unreachable();
}

static inline void setupfileinput(const char *const fstr)
{
	ERRNO_T myerrno;

	#ifdef _WIN32
	LARGE_INTEGER fsize;

	fd = CreateFile(fstr, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (fd == INVALID_HANDLE_VALUE)
	{
		myerrno = GetLastError();
		printf("CreateFile() failed ecode 0x%lx" NEWLINE, myerrno);
		ExitProcess(myerrno);
	}

	if (!GetFileSizeEx(fd, &fsize))
	{
		myerrno = GetLastError();
		printf("GetLastError() failed ecode 0x%lx" NEWLINE, myerrno);
		ExitProcess(myerrno);
	}

	size = fsize.QuadPart;
	#else
	struct stat fst;

	fd = open(fstr, O_RDONLY);
	if (fd == -1)
	{
		myerrno = errno;
		perror("input file open()");
		_exit(myerrno);
	}

	if (fstat(fd, &fst))
	{
		myerrno = errno;
		perror("input file fstat()");
		_exit(myerrno);
	}

	size = fst.st_size;
	#endif
}

static inline void setupfileoutput(const char *const fstr)
{
	ERRNO_T myerrno;

	#ifdef _WIN32
	LARGE_INTEGER li;

	fd = CreateFile(fstr, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (fd == INVALID_HANDLE_VALUE)
	{
		myerrno = GetLastError();
		printf("CreateFile() failed ecode 0x%lx" NEWLINE, myerrno);
		ExitProcess(myerrno);
	}

	if (size)
	{
		li.QuadPart = size;
		if (!SetFilePointerEx(fd, li, NULL, FILE_BEGIN))
		{
			myerrno = GetLastError();
			printf("SetFilePointerEx() failed ecode 0x%lx" NEWLINE, myerrno);
			ExitProcess(myerrno);
		}

		if (!SetEndOfFile(fd))
		{
			myerrno = GetLastError();
			printf("SetEndOfFile() failed ecode 0x%lx" NEWLINE, myerrno);
			ExitProcess(myerrno);
		}

		li.QuadPart = 0;
		if (!SetFilePointerEx(fd, li, NULL, FILE_BEGIN))
		{
			myerrno = GetLastError();
			printf("SetFilePointerEx() failed ecode 0x%lx" NEWLINE, myerrno);
			ExitProcess(myerrno);
		}
	}
	#else
	fd = open(fstr, O_WRONLY | O_EXCL | O_CREAT, 0644);
	if (fd == -1)
	{
		myerrno = errno;
		perror("output file open()");
		_exit(myerrno);
	}

	if (size)
	{
		if (fallocate(fd, 0, 0, size))
		{
			myerrno = errno;
			perror("output file fallocate()");
			_exit(myerrno);
		}
	}
	#endif
}

#ifdef _WIN32
static inline void spipe()
{
}

static inline void setuppipe()
{
}
#else
void setuppipe(int argc, char *argv[])
{
	ERRNO_T myerrno;
	pid_t p;
	int pfds[2];
	char **cmd;
	struct clone_args cargs = {0};
	int x;

	if (pipe(pfds))
	{
		myerrno = errno;
		perror("pipe()");
		_exit(myerrno);
	}

	cargs.flags = CLONE_PIDFD;
	cargs.pidfd = (__u64)&pidfd;
        p = syscall(SYS_clone3, &cargs, sizeof(struct clone_args));
	if (p < 0)
	{
		myerrno = errno;
		perror("clone3()");
		_exit(myerrno);
	}
	if (!p)
	{
		close(pfds[0]);
		if (dup2(pfds[1], STDOUT_FILENO) != STDOUT_FILENO)
		{
			myerrno = errno;
			perror("dup2()");
			_exit(myerrno);
		}
		close(pfds[1]);
		cmd = __builtin_alloca(sizeof(char*) * (1 + (argc - 5)));
		x = 5;
		while (x != argc)
		{
			cmd[x-5] = argv[x];
			x++;
		}
		cmd[x-5] = NULL;
		execvp(argv[5], cmd);
		myerrno = errno;
		perror("execvp()");
		_exit(myerrno);
	}
	close(pfds[1]);
	pfd = pfds[0];
	child = p;
}

static inline char *exittypestr(unsigned long t)
{
	switch (t)
	{
	case CLD_EXITED:
		return "Process voluntarily exited";
	case CLD_KILLED:
		return "Process killed by signal!";
	case CLD_DUMPED:
		return "Process killed by signal! Core dumped!";
	case CLD_STOPPED:
		return "Process stoped by signal";
	case CLD_TRAPPED:
		return "Process is being traced and has been trapped";
	case CLD_CONTINUED:
		return "Process has been continued";
	default:
		return "ERROR UNKNOWN EXIT CAUSE";
	}
}


static inline void spipe()
{
	ERRNO_T myerrno;
	char *buf;
	siginfo_t si;
	ssize_t retval;
	ssize_t totsent = 0;

	size = 0;
	
	if (send(sock, &size, sizeof(uint64_t), MSG_MORE ) != sizeof(uint64_t))
	{
		SOCKERROR("send()");
	}

	buf = mmap(NULL, BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (buf == MAP_FAILED)
	{
		myerrno = errno;
		perror("mmap()");
		_exit(myerrno);
	}
	while (1)
	{
		retval = read(pfd, buf, BUFFER_SIZE);
		if (retval < 0)
		{
			myerrno = errno;
			perror("pipe read()");
			_exit(myerrno);
		}
		if (!retval)
		{
			FASTPRINT("Pipe closed." NEWLINE "Waiting for the process to exit...");
			if (waitid(P_PIDFD, pidfd, &si, WEXITED))
			{
				myerrno = errno;
				perror("waitid()");
				_exit(myerrno);
			}
			printf(" done!" NEWLINE "\tChild exit type: %s" NEWLINE "\tChild exit code: %i" NEWLINE "Sent " U64_PF " bytes" NEWLINE, exittypestr(si.si_code), si.si_status, totsent);
			close(pidfd);
			CLOSESOCK(sock);
			_exit(0);
		}
		if (send(sock, buf, retval, 0) != retval)
		{
			SOCKERROR("send()");
		}
		totsent += retval;
	}
}
#endif

static inline void setupstate(const char * const instr)
{
	if (__builtin_strlen(instr) != 2)
	{
		printusage();
		exitbadusage();
	}

	if (instr[0] == 's')
	{
		state |= FLAG_TCP_SERVER;
	}
	else if (instr[0] != 'c')
	{
		printusage();
		exitbadusage();
	}

	if (instr[1] == 's')
	{
		state |= FLAG_SENDER;
	}
	else if (instr[1] != 'r')
	{
		printusage();
		exitbadusage();
	}

	return;
}

static inline void setupaddress(const char *const addr, const char *const ports)
{
	unsigned long iport;
	char *endptr;

	#if ((defined(_WIN32)) && (WINVER < 0x0600))
	char *term;
	NTSTATUS retval;

	retval = RtlIpv4StringToAddress(addr, 1, &term, (IN_ADDR *) &ip);
	if ((retval != STATUS_SUCCESS) || (*term != 0))
	{
		FASTPRINT("Failed to parse ipv4 address" NEWLINE);
	        exitbadusage();
	}
	#else
	if (!inet_pton(AF_INET, addr, &ip))
	{
		FASTPRINT("Failed to parse ipv4 address" NEWLINE);
	        exitbadusage();
	}
	#endif
	
	iport = strtoul(ports, &endptr, 0);
	if ((*endptr) || (iport > 0xffff))
	{
		FASTPRINT("bad port number!" NEWLINE);
	        exitbadusage();
	}
	port = iport;
}

static inline void rfile(const char *const fname)
{
	ERRNO_T myerrno;
	char *buf;
	ssize_t gnutlsretval;
	size_t totrecv = 0;
	#ifdef _WIN32
	//HANDLE fmap;
	//FILE_DISPOSITION_INFO fdi = 0;
	DWORD written;
	int wsaerrno;
	#endif

	if (recv(sock, (char *) &size, sizeof(uint64_t), MSG_WAITALL) != sizeof(uint64_t))
	{
		SOCKERROR("recv()");
	}
	#ifdef __ORDER_LITTLE_ENDIAN__
	size = __builtin_bswap64(size);
	#endif

	if (size)
	{
		printf("Expecting " U64_PF " bytes..." NEWLINE, size);
	}
	else
	{
	        FASTPRINT("Receiving a file of unknown size..." NEWLINE);
	}
	
	setupfileoutput(fname);

	#ifdef _WIN32
	/* fmap = CreateFileMapping(fd, NULL, PAGE_READWRITE, 0, 0, NULL); */
	/* if (!fmap) */
	/* { */
	/* 	myerrno = GetLastError(); */
	/* 	printf("CreateFileMapping() failed ecode 0x%lx" NEWLINE "Marking file for deletion...", myerrno); */
	/* 	/\* fdi.DeleteFile = 1; *\/ */
	/* 	/\* if (!SetFileInformationByHandle(fd, FileDispositionInfo, &fdi, sizeof(FILE_DISPOSITION_INFO))) *\/ */
	/* 	/\* { *\/ */
	/* 	/\* 	myerrno = GetLastError(); *\/ */
	/* 	/\* 	printf("CreateFileMapping() failed ecode 0x%lx" NEWLINE "Unable to delete!", myerrno); *\/ */
	/* 	/\* } *\/ */
	/* 	/\* CLOSEFILE(fd); *\/ */
	/* 	FASTEXIT(myerrno); */
	/* } */
	/* buf = MapViewOfFile(fmap, FILE_MAP_WRITE | FILE_MAP_READ, 0, 0, 0); */
	/* if (!buf) */
	/* { */
	/* 	myerrno = GetLastError(); */
	/* 	printf("MapViewOfFile() failed ecode 0x%lx" NEWLINE "Marking file for deletion...", myerrno); */
	/* 	/\* fdi.DeleteFile = 1; *\/ */
	/* 	/\* if (!SetFileInformationByHandle(fd, FileDispositionInfo, &fdi, sizeof(FILE_DISPOSITION_INFO))) *\/ */
	/* 	/\* { *\/ */
	/* 	/\* 	myerrno = GetLastError(); *\/ */
	/* 	/\* 	printf("CreateFileMapping() failed ecode 0x%lx" NEWLINE "Unable to delete!", myerrno); *\/ */
	/* 	/\* } *\/ */
	/* 	/\* CLOSEFILE(fd); *\/ */
	/* 	FASTEXIT(myerrno); */
	/* } */
	buf = VirtualAlloc(NULL, BUFFER_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (!buf)
	{
		myerrno = GetLastError();
		printf("VirtualAlloc() 0x%lx" NEWLINE, myerrno);
		FASTEXIT(myerrno);
	}
	#else
	buf = mmap(NULL, BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (buf == MAP_FAILED)
	{
		myerrno = errno;
		perror("mmap()");
		FASTEXIT(myerrno);
	}
	#endif
	while (1)
	{
		gnutlsretval = recv(sock, buf, BUFFER_SIZE, 0);
		if (gnutlsretval < 0)
		{
			SOCKERROR("recv()");
		}
		if (gnutlsretval == 0)
		{
			printf("Got EOF from peer. Received " U64_PF " bytes total" NEWLINE, totrecv);
			break;
		}
		#ifdef _WIN32
		if (!WriteFile(fd, buf, gnutlsretval, &written, NULL))
		{
			myerrno = GetLastError();
			printf("WriteFile() failed ecode 0x%lx" NEWLINE, myerrno);
			FASTEXIT(myerrno);
		}
		#else
		if (write(fd, buf, gnutlsretval) != gnutlsretval)
		{
			myerrno = errno;
			perror("output file write()");
			FASTEXIT(myerrno);
		}
		#endif
		totrecv += gnutlsretval;
		if (size)
		{
			if (totrecv == size)
			{
				printf("Completed recieving the file, got all " U64_PF " bytes" NEWLINE, size);
				break;
			}
		}
	}
	CLOSESOCK(sock);
	#ifdef _WIN32
	/* if (!UnmapViewOfFile(buf)) */
	/* { */
	/* 	myerrno = GetLastError(); */
	/* 	printf("UnmapViewOfFile() failed ecode 0x%lx" NEWLINE, myerrno); */
	/* } */
	/* CloseHandle(fmap); */

	if (!VirtualFree(buf, 0, MEM_RELEASE))
	{
		myerrno = GetLastError();
		printf("VirtualFree() 0x%lx" NEWLINE, myerrno);
		FASTEXIT(myerrno);
	}
	#endif
	CLOSEFILE(fd);
	return;

	FASTEXIT(0);
	__builtin_unreachable();
}

static inline void sfile()
{
	ssize_t totsent = 0;
	ssize_t cursend;
	#ifdef _WIN32
	WSABUF wsab;
	int wsaerrno;
	DWORD sentbytes;
	LARGE_INTEGER li;
	//#else
	//ERRNO_T myerrno;
	#endif
	ERRNO_T myerrno;

	#ifdef __ORDER_LITTLE_ENDIAN__
	size = __builtin_bswap64(size);
	#endif
	SEND_MSG_MORE(&size, sizeof(uint64_t));
	
	#ifdef __ORDER_LITTLE_ENDIAN__
	size = __builtin_bswap64(size);
	#endif

	while (((size_t)totsent) != size)
	{
		cursend = size - totsent;
		//this macro is changed for win32
		//not a mistake!
		if (cursend > BERNTRNSFR_LINUX_SENDFILE_MAX)
		{
			cursend = BERNTRNSFR_LINUX_SENDFILE_MAX;
		}

		#ifdef _WIN32
		//Fucking stupid hack because I don't feel like dealing with
		//overlapped io right now
		//why the fuck can't this shit work like sendfile()
		//bunch of extra kernel user transitions for this shit
		//I think even if overlapped
		//
		//on windows 11:
		//and you know what is even stupider?!
		//Originally I was using SetFilePointerEX(FILE_CURRENT)
		//after TransmitFile() to move the file pointer,
		//but that wasn't working, after debugging I discovered that
		//the first call to TransmitFile() will move the file pointer by
		//32768 bytes (0x8000 bytes). Subsequent calls to transmit file
		//won't move the pointer. None of this is in the documentation
		//and as far as I can tell using google I'm the first person to
		//notice this shit.
		li.QuadPart = totsent;
		if (!SetFilePointerEx(fd, li, NULL, FILE_BEGIN))
		{
			myerrno = GetLastError();
			printf("SetFilePointerEx() failed ecode 0x%lx" NEWLINE, myerrno);
			ExitProcess(myerrno);
		}
		if (!TransmitFile(sock, fd, cursend, 0, NULL, NULL, 0))
		{
			SOCKERROR("TransmitFile()");
		}
		#else
		if (sendfile(sock, fd, NULL, cursend) != cursend)
		{
			SOCKERROR("sendfile()");
		}
		#endif
		totsent += cursend;
	}
	printf("Completed sending the file, sent all " U64_PF " bytes" NEWLINE, size);

	CLOSESOCK(sock);
        CLOSEFILE(fd);
}

int main(int argc, char *argv[])
{
	ERRNO_T myerrno;

	#ifdef _WIN32
	if (!SetPriorityClass(GetCurrentProcess(), IDLE_PRIORITY_CLASS))
	{
		myerrno = GetLastError();
		printf("failed SetPriorityClass(), ecode 0x%lx" NEWLINE, myerrno);
		return myerrno;
	}
	//if they make GetCurrentProcess() not a psuedo handle we leak a handle here
	//todo fix this in case they change this
	//also todo if the ntsetinformationprocess() for io priority is exposed
	//to win32 do that to reduce io priority

	stdouth = GetStdHandle(STD_OUTPUT_HANDLE);
	if (stdouth == INVALID_HANDLE_VALUE)
	{
		myerrno = GetLastError();
		printf("failed GetStdHandle(), ecode 0x%lx" NEWLINE, myerrno);
		return myerrno;
	}
	#else
	struct sched_param sp = {0};

	if (sched_setscheduler(0, SCHED_IDLE, &sp))
	{
		myerrno = errno;
		perror("sched_setscheduler()");
		return myerrno;
	}

	//maybe todo ioprio_set()
	
	setlocale(LC_ALL, "");

	if (sigaction(SIGPIPE, &siga, NULL))
	{
		myerrno = errno;
		perror("sigaction(SIGPIPE)");
		return myerrno;
	}
	#endif

	if (setvbuf(stdout, NULL, _IONBF, 0))
	{
		puts("setvbuf() failed");
	}

	if (argc < 5)
	{
		printusage();
		exitbadusage();
	}

	setupstate(argv[1]);
	setupaddress(argv[2], argv[3]);

	if (state & FLAG_SENDER)
	{
		if ((*((uint16_t*)argv[4])) == PIPE_STR)
		{
			#ifdef _WIN32
			FASTPRINT("Pipe sources are not implemented on Windows" NEWLINE);
			return ERROR_CALL_NOT_IMPLEMENTED;
			#endif
			if (argc < 6)
			{
			        FASTPRINT("you must provide a command that whose output will be transmitted!" NEWLINE);
				exitbadusage();
			}
			state |= FLAG_PIPE;
		}
		else
		{
			setupfileinput(argv[4]);
		}
	}
	else
	{
		if (argc > 5)
		{
			FASTPRINT("you have given too many arguments!");
			exitbadusage();
		}
	}

	FASTPRINT("Connecting socket...");
	setupsocket();
	FASTPRINT(" done!" NEWLINE NEWLINE);
	if (state & FLAG_SENDER)
	{
		if (state & FLAG_PIPE)
		{
			//do pipe
			setuppipe(argc, argv);
			spipe();
		}
		else
		{
			sfile();
		}
	}
	else
	{
		rfile(argv[4]);
	}

	return 0;
}
