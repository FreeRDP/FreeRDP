/**
 * WinPR: Windows Portable Runtime
 * Windows Sockets (Winsock)
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef WINPR_WINSOCK_H
#define WINPR_WINSOCK_H

#include <winpr/winpr.h>
#include <winpr/wtypes.h>
#include <winpr/windows.h>

#ifdef WINPR_MONO_CONFLICT
#define closesocket winpr_closesocket
#endif

#ifdef _WIN32

#define _accept			accept
#define _bind			bind
#define _connect		connect
#define _ioctlsocket		ioctlsocket
#define _getpeername		getpeername
#define _getsockname		getsockname
#define _getsockopt		getsockopt
#define _htonl			htonl
#define _htons			htons
#define _inet_addr		inet_addr
#define _inet_ntoa		inet_ntoa
#define _listen			listen
#define _ntohl			ntohl
#define _ntohs			ntohs
#define _recv			recv
#define _recvfrom		recvfrom
#define _select			select
#define _send			send
#define _sendto			sendto
#define _setsockopt		setsockopt
#define _shutdown		shutdown
#define _socket			socket
#define _gethostbyaddr		gethostbyaddr
#define _gethostbyname		gethostbyname
#define _gethostname		gethostname
#define _getservbyport		getservbyport
#define _getservbyname		getservbyname
#define _getprotobynumber	getprotobynumber
#define _getprotobyname		getprotobyname

#define _IFF_UP			IFF_UP
#define _IFF_BROADCAST		IFF_BROADCAST
#define _IFF_LOOPBACK		IFF_LOOPBACK
#define _IFF_POINTTOPOINT	IFF_POINTTOPOINT
#define _IFF_MULTICAST		IFF_MULTICAST

#if (_WIN32_WINNT < 0x0600)

WINPR_API PCSTR winpr_inet_ntop(INT Family, PVOID pAddr, PSTR pStringBuf, size_t StringBufSize);
WINPR_API INT winpr_inet_pton(INT Family, PCSTR pszAddrString, PVOID pAddrBuf);

#define inet_ntop winpr_inet_ntop
#define inet_pton winpr_inet_pton


#endif /* (_WIN32_WINNT < 0x0600) */

#else /* _WIN32 */

#include <netdb.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <net/if.h>

#include <winpr/io.h>
#include <winpr/error.h>
#include <winpr/platform.h>

#define WSAEVENT	HANDLE
#define LPWSAEVENT	LPHANDLE
#define WSAOVERLAPPED	OVERLAPPED
typedef OVERLAPPED*	LPWSAOVERLAPPED;

typedef UINT_PTR SOCKET;
typedef struct sockaddr_storage SOCKADDR_STORAGE;

#ifndef INVALID_SOCKET
#define INVALID_SOCKET (SOCKET)(~0)
#endif

#define WSADESCRIPTION_LEN	256
#define WSASYS_STATUS_LEN	128

#define FD_READ_BIT				0
#define FD_READ					(1 << FD_READ_BIT)

#define FD_WRITE_BIT				1
#define FD_WRITE				(1 << FD_WRITE_BIT)

#define FD_OOB_BIT				2
#define FD_OOB					(1 << FD_OOB_BIT)

#define FD_ACCEPT_BIT				3
#define FD_ACCEPT				(1 << FD_ACCEPT_BIT)

#define FD_CONNECT_BIT				4
#define FD_CONNECT				(1 << FD_CONNECT_BIT)

#define FD_CLOSE_BIT				5
#define FD_CLOSE				(1 << FD_CLOSE_BIT)

#define FD_QOS_BIT				6
#define FD_QOS					(1 << FD_QOS_BIT)

#define FD_GROUP_QOS_BIT			7
#define FD_GROUP_QOS				(1 << FD_GROUP_QOS_BIT)

#define FD_ROUTING_INTERFACE_CHANGE_BIT		8
#define FD_ROUTING_INTERFACE_CHANGE		(1 << FD_ROUTING_INTERFACE_CHANGE_BIT)

#define FD_ADDRESS_LIST_CHANGE_BIT		9
#define FD_ADDRESS_LIST_CHANGE			(1 << FD_ADDRESS_LIST_CHANGE_BIT)

#define FD_MAX_EVENTS				10
#define FD_ALL_EVENTS				((1 << FD_MAX_EVENTS) - 1)

#define SD_RECEIVE		0
#define SD_SEND			1
#define SD_BOTH			2

#define SOCKET_ERROR		(-1)

typedef struct WSAData
{
	WORD wVersion;
	WORD wHighVersion;
#ifdef _M_AMD64
	unsigned short iMaxSockets;
	unsigned short iMaxUdpDg;
	char* lpVendorInfo;
	char szDescription[WSADESCRIPTION_LEN+1];
	char szSystemStatus[WSASYS_STATUS_LEN+1];
#else
	char szDescription[WSADESCRIPTION_LEN+1];
	char szSystemStatus[WSASYS_STATUS_LEN+1];
	unsigned short iMaxSockets;
	unsigned short iMaxUdpDg;
	char* lpVendorInfo;
#endif
} WSADATA, *LPWSADATA;

#ifndef MAKEWORD
#define MAKEWORD(a,b) ((WORD)(((BYTE)((DWORD_PTR)(a) & 0xFF)) | (((WORD)((BYTE)((DWORD_PTR)(b) & 0xFF))) << 8)))
#endif

typedef struct in6_addr IN6_ADDR;
typedef struct in6_addr* PIN6_ADDR;
typedef struct in6_addr* LPIN6_ADDR;

struct sockaddr_in6_old
{
	SHORT sin6_family;
	USHORT sin6_port;
	ULONG sin6_flowinfo;
	IN6_ADDR sin6_addr;
};

typedef union sockaddr_gen
{
	struct sockaddr Address;
	struct sockaddr_in AddressIn;
	struct sockaddr_in6_old AddressIn6;
} sockaddr_gen;

#define _IFF_UP			0x00000001
#define _IFF_BROADCAST		0x00000002
#define _IFF_LOOPBACK		0x00000004
#define _IFF_POINTTOPOINT	0x00000008
#define _IFF_MULTICAST		0x00000010

struct _INTERFACE_INFO
{
	ULONG iiFlags;
	sockaddr_gen iiAddress;
	sockaddr_gen iiBroadcastAddress;
	sockaddr_gen iiNetmask;
};
typedef struct _INTERFACE_INFO INTERFACE_INFO;
typedef INTERFACE_INFO* LPINTERFACE_INFO;

#define MAX_PROTOCOL_CHAIN	7
#define WSAPROTOCOL_LEN		255

typedef struct _WSAPROTOCOLCHAIN
{
	int ChainLen;
	DWORD ChainEntries[MAX_PROTOCOL_CHAIN];
}
WSAPROTOCOLCHAIN, *LPWSAPROTOCOLCHAIN;

typedef struct _WSAPROTOCOL_INFOA
{
	DWORD dwServiceFlags1;
	DWORD dwServiceFlags2;
	DWORD dwServiceFlags3;
	DWORD dwServiceFlags4;
	DWORD dwProviderFlags;
	GUID ProviderId;
	DWORD dwCatalogEntryId;
	WSAPROTOCOLCHAIN ProtocolChain;
	int iVersion;
	int iAddressFamily;
	int iMaxSockAddr;
	int iMinSockAddr;
	int iSocketType;
	int iProtocol;
	int iProtocolMaxOffset;
	int iNetworkByteOrder;
	int iSecurityScheme;
	DWORD dwMessageSize;
	DWORD dwProviderReserved;
	CHAR szProtocol[WSAPROTOCOL_LEN+1];
}
WSAPROTOCOL_INFOA, *LPWSAPROTOCOL_INFOA;

typedef struct _WSAPROTOCOL_INFOW
{
	DWORD dwServiceFlags1;
	DWORD dwServiceFlags2;
	DWORD dwServiceFlags3;
	DWORD dwServiceFlags4;
	DWORD dwProviderFlags;
	GUID ProviderId;
	DWORD dwCatalogEntryId;
	WSAPROTOCOLCHAIN ProtocolChain;
	int iVersion;
	int iAddressFamily;
	int iMaxSockAddr;
	int iMinSockAddr;
	int iSocketType;
	int iProtocol;
	int iProtocolMaxOffset;
	int iNetworkByteOrder;
	int iSecurityScheme;
	DWORD dwMessageSize;
	DWORD dwProviderReserved;
	WCHAR szProtocol[WSAPROTOCOL_LEN+1];
}
WSAPROTOCOL_INFOW, *LPWSAPROTOCOL_INFOW;

typedef void (CALLBACK * LPWSAOVERLAPPED_COMPLETION_ROUTINE)(DWORD dwError,
		DWORD cbTransferred, LPWSAOVERLAPPED lpOverlapped, DWORD dwFlags);

typedef UINT32 GROUP;
#define SG_UNCONSTRAINED_GROUP		0x01
#define SG_CONSTRAINED_GROUP		0x02

#define SIO_GET_INTERFACE_LIST		_IOR('t', 127, ULONG)
#define SIO_GET_INTERFACE_LIST_EX	_IOR('t', 126, ULONG)
#define SIO_SET_MULTICAST_FILTER	_IOW('t', 125, ULONG)
#define SIO_GET_MULTICAST_FILTER	_IOW('t', 124 | IOC_IN, ULONG)
#define SIOCSIPMSFILTER			SIO_SET_MULTICAST_FILTER
#define SIOCGIPMSFILTER			SIO_GET_MULTICAST_FILTER

#ifdef UNICODE
#define WSAPROTOCOL_INFO	WSAPROTOCOL_INFOW
#define LPWSAPROTOCOL_INFO	LPWSAPROTOCOL_INFOW
#else
#define WSAPROTOCOL_INFO	WSAPROTOCOL_INFOA
#define LPWSAPROTOCOL_INFO	LPWSAPROTOCOL_INFOA
#endif

#ifdef __cplusplus
extern "C" {
#endif

WINPR_API int WSAStartup(WORD wVersionRequired, LPWSADATA lpWSAData);
WINPR_API int WSACleanup(void);

WINPR_API void WSASetLastError(int iError);
WINPR_API int WSAGetLastError(void);

WINPR_API HANDLE WSACreateEvent(void);
WINPR_API BOOL WSASetEvent(HANDLE hEvent);
WINPR_API BOOL WSAResetEvent(HANDLE hEvent);
WINPR_API BOOL WSACloseEvent(HANDLE hEvent);

WINPR_API int WSAEventSelect(SOCKET s, WSAEVENT hEventObject, LONG lNetworkEvents);

WINPR_API DWORD WSAWaitForMultipleEvents(DWORD cEvents,
		const HANDLE* lphEvents, BOOL fWaitAll, DWORD dwTimeout, BOOL fAlertable);

WINPR_API SOCKET WSASocketA(int af, int type, int protocol, LPWSAPROTOCOL_INFOA lpProtocolInfo, GROUP g, DWORD dwFlags);
WINPR_API SOCKET WSASocketW(int af, int type, int protocol, LPWSAPROTOCOL_INFOW lpProtocolInfo, GROUP g, DWORD dwFlags);

WINPR_API int WSAIoctl(SOCKET s, DWORD dwIoControlCode, LPVOID lpvInBuffer,
		DWORD cbInBuffer, LPVOID lpvOutBuffer, DWORD cbOutBuffer,
		LPDWORD lpcbBytesReturned, LPWSAOVERLAPPED lpOverlapped,
		LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine);

WINPR_API SOCKET _accept(SOCKET s, struct sockaddr* addr, int* addrlen);
WINPR_API int _bind(SOCKET s, const struct sockaddr* addr, int namelen);
WINPR_API int closesocket(SOCKET s);
WINPR_API int _connect(SOCKET s, const struct sockaddr* name, int namelen);
WINPR_API int _ioctlsocket(SOCKET s, long cmd, u_long* argp);
WINPR_API int _getpeername(SOCKET s, struct sockaddr* name, int* namelen);
WINPR_API int _getsockname(SOCKET s, struct sockaddr* name, int* namelen);
WINPR_API int _getsockopt(SOCKET s, int level, int optname, char* optval, int* optlen);
WINPR_API u_long _htonl(u_long hostlong);
WINPR_API u_short _htons(u_short hostshort);
WINPR_API unsigned long _inet_addr(const char* cp);
WINPR_API char* _inet_ntoa(struct in_addr in);
WINPR_API int _listen(SOCKET s, int backlog);
WINPR_API u_long _ntohl(u_long netlong);
WINPR_API u_short _ntohs(u_short netshort);
WINPR_API int _recv(SOCKET s, char* buf, int len, int flags);
WINPR_API int _recvfrom(SOCKET s, char* buf, int len, int flags, struct sockaddr* from, int* fromlen);
WINPR_API int _select(int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds, const struct timeval* timeout);
WINPR_API int _send(SOCKET s, const char* buf, int len, int flags);
WINPR_API int _sendto(SOCKET s, const char* buf, int len, int flags, const struct sockaddr* to, int tolen);
WINPR_API int _setsockopt(SOCKET s, int level, int optname, const char* optval, int optlen);
WINPR_API int _shutdown(SOCKET s, int how);
WINPR_API SOCKET _socket(int af, int type, int protocol);
WINPR_API struct hostent* _gethostbyaddr(const char* addr, int len, int type);
WINPR_API struct hostent* _gethostbyname(const char* name);
WINPR_API int _gethostname(char* name, int namelen);
WINPR_API struct servent* _getservbyport(int port, const char* proto);
WINPR_API struct servent* _getservbyname(const char* name, const char* proto);
WINPR_API struct protoent* _getprotobynumber(int number);
WINPR_API struct protoent* _getprotobyname(const char* name);

#ifdef __cplusplus
}
#endif

#ifdef UNICODE
#define WSASocket	WSASocketW
#else
#define WSASocket	WSASocketA
#endif

#endif /* _WIN32 */

#endif /* WINPR_WINSOCK_H */

