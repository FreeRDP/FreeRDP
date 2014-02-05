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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/winsock.h>

/**
 * ws2_32.dll:
 * 
 * __WSAFDIsSet
 * accept
 * bind
 * closesocket
 * connect
 * freeaddrinfo
 * FreeAddrInfoEx
 * FreeAddrInfoExW
 * FreeAddrInfoW
 * getaddrinfo
 * GetAddrInfoExA
 * GetAddrInfoExCancel
 * GetAddrInfoExOverlappedResult
 * GetAddrInfoExW
 * GetAddrInfoW
 * gethostbyaddr
 * gethostbyname
 * gethostname
 * GetHostNameW
 * getnameinfo
 * GetNameInfoW
 * getpeername
 * getprotobyname
 * getprotobynumber
 * getservbyname
 * getservbyport
 * getsockname
 * getsockopt
 * htonl
 * htons
 * inet_addr
 * inet_ntoa
 * inet_ntop
 * inet_pton
 * InetNtopW
 * InetPtonW
 * ioctlsocket
 * listen
 * ntohl
 * ntohs
 * recv
 * recvfrom
 * select
 * send
 * sendto
 * SetAddrInfoExA
 * SetAddrInfoExW
 * setsockopt
 * shutdown
 * socket
 * WahCloseApcHelper
 * WahCloseHandleHelper
 * WahCloseNotificationHandleHelper
 * WahCloseSocketHandle
 * WahCloseThread
 * WahCompleteRequest
 * WahCreateHandleContextTable
 * WahCreateNotificationHandle
 * WahCreateSocketHandle
 * WahDestroyHandleContextTable
 * WahDisableNonIFSHandleSupport
 * WahEnableNonIFSHandleSupport
 * WahEnumerateHandleContexts
 * WahInsertHandleContext
 * WahNotifyAllProcesses
 * WahOpenApcHelper
 * WahOpenCurrentThread
 * WahOpenHandleHelper
 * WahOpenNotificationHandleHelper
 * WahQueueUserApc
 * WahReferenceContextByHandle
 * WahRemoveHandleContext
 * WahWaitForNotification
 * WahWriteLSPEvent
 * WEP
 * WPUCompleteOverlappedRequest
 * WPUGetProviderPathEx
 * WSAAccept
 * WSAAddressToStringA
 * WSAAddressToStringW
 * WSAAdvertiseProvider
 * WSAAsyncGetHostByAddr
 * WSAAsyncGetHostByName
 * WSAAsyncGetProtoByName
 * WSAAsyncGetProtoByNumber
 * WSAAsyncGetServByName
 * WSAAsyncGetServByPort
 * WSAAsyncSelect
 * WSACancelAsyncRequest
 * WSACancelBlockingCall
 * WSACleanup
 * WSACloseEvent
 * WSAConnect
 * WSAConnectByList
 * WSAConnectByNameA
 * WSAConnectByNameW
 * WSACreateEvent
 * WSADuplicateSocketA
 * WSADuplicateSocketW
 * WSAEnumNameSpaceProvidersA
 * WSAEnumNameSpaceProvidersExA
 * WSAEnumNameSpaceProvidersExW
 * WSAEnumNameSpaceProvidersW
 * WSAEnumNetworkEvents
 * WSAEnumProtocolsA
 * WSAEnumProtocolsW
 * WSAEventSelect
 * WSAGetLastError
 * WSAGetOverlappedResult
 * WSAGetQOSByName
 * WSAGetServiceClassInfoA
 * WSAGetServiceClassInfoW
 * WSAGetServiceClassNameByClassIdA
 * WSAGetServiceClassNameByClassIdW
 * WSAHtonl
 * WSAHtons
 * WSAInstallServiceClassA
 * WSAInstallServiceClassW
 * WSAIoctl
 * WSAIsBlocking
 * WSAJoinLeaf
 * WSALookupServiceBeginA
 * WSALookupServiceBeginW
 * WSALookupServiceEnd
 * WSALookupServiceNextA
 * WSALookupServiceNextW
 * WSANSPIoctl
 * WSANtohl
 * WSANtohs
 * WSAPoll
 * WSAProviderCompleteAsyncCall
 * WSAProviderConfigChange
 * WSApSetPostRoutine
 * WSARecv
 * WSARecvDisconnect
 * WSARecvFrom
 * WSARemoveServiceClass
 * WSAResetEvent
 * WSASend
 * WSASendDisconnect
 * WSASendMsg
 * WSASendTo
 * WSASetBlockingHook
 * WSASetEvent
 * WSASetLastError
 * WSASetServiceA
 * WSASetServiceW
 * WSASocketA
 * WSASocketW
 * WSAStartup
 * WSAStringToAddressA
 * WSAStringToAddressW
 * WSAUnadvertiseProvider
 * WSAUnhookBlockingHook
 * WSAWaitForMultipleEvents
 * WSCDeinstallProvider
 * WSCDeinstallProviderEx
 * WSCEnableNSProvider
 * WSCEnumProtocols
 * WSCEnumProtocolsEx
 * WSCGetApplicationCategory
 * WSCGetApplicationCategoryEx
 * WSCGetProviderInfo
 * WSCGetProviderPath
 * WSCInstallNameSpace
 * WSCInstallNameSpaceEx
 * WSCInstallNameSpaceEx2
 * WSCInstallProvider
 * WSCInstallProviderAndChains
 * WSCInstallProviderEx
 * WSCSetApplicationCategory
 * WSCSetApplicationCategoryEx
 * WSCSetProviderInfo
 * WSCUnInstallNameSpace
 * WSCUnInstallNameSpaceEx2
 * WSCUpdateProvider
 * WSCUpdateProviderEx
 * WSCWriteNameSpaceOrder
 * WSCWriteProviderOrder
 * WSCWriteProviderOrderEx
 */

#ifdef _WIN32

#if (_WIN32_WINNT < 0x0600)

PCSTR inet_ntop(INT Family, PVOID pAddr, PSTR pStringBuf, size_t StringBufSize)
{
	if (Family == AF_INET)
	{
		struct sockaddr_in in;

		memset(&in, 0, sizeof(in));
		in.sin_family = AF_INET;
		memcpy(&in.sin_addr, pAddr, sizeof(struct in_addr));
		getnameinfo((struct sockaddr*) &in, sizeof(struct sockaddr_in), pStringBuf, StringBufSize, NULL, 0, NI_NUMERICHOST);

		return pStringBuf;
	}
	else if (Family == AF_INET6)
	{
		struct sockaddr_in6 in;

		memset(&in, 0, sizeof(in));
		in.sin6_family = AF_INET6;
		memcpy(&in.sin6_addr, pAddr, sizeof(struct in_addr6));
		getnameinfo((struct sockaddr*) &in, sizeof(struct sockaddr_in6), pStringBuf, StringBufSize, NULL, 0, NI_NUMERICHOST);

		return pStringBuf;
	}

	return NULL;
}

#endif /* (_WIN32_WINNT < 0x0600) */

#else /* _WIN32 */

int WSAStartup(WORD wVersionRequired, LPWSADATA lpWSAData)
{
	return 0;
}

int WSACleanup(void)
{
	return 0;
}

void WSASetLastError(int iError)
{

}

int WSAGetLastError(void)
{
	return 0;
}

SOCKET _accept(SOCKET s, struct sockaddr* addr, int* addrlen)
{
	return 0;
}

int _bind(SOCKET s, const struct sockaddr* addr, int namelen)
{
	return 0;
}

int closesocket(SOCKET s)
{
	return 0;
}

int _connect(SOCKET s, const struct sockaddr* name, int namelen)
{
	return 0;
}

int _ioctlsocket(SOCKET s, long cmd, u_long* argp)
{
	return 0;
}

int _getpeername(SOCKET s, struct sockaddr* name, int* namelen)
{
	return 0;
}

int _getsockname(SOCKET s, struct sockaddr* name, int* namelen)
{
	return 0;
}

int _getsockopt(SOCKET s, int level, int optname, char* optval, int* optlen)
{
	return 0;
}

u_long _htonl(u_long hostlong)
{
	return 0;
}

u_short _htons(u_short hostshort)
{
	return 0;
}

unsigned long _inet_addr(const char* cp)
{
	return 0;
}

char* _inet_ntoa(struct in_addr in)
{
	return 0;
}

int _listen(SOCKET s, int backlog)
{
	return 0;
}

u_long _ntohl(u_long netlong)
{
	return 0;
}

u_short _ntohs(u_short netshort)
{
	return 0;
}

int _recv(SOCKET s, char* buf, int len, int flags)
{
	return 0;
}

int _recvfrom(SOCKET s, char* buf, int len, int flags, struct sockaddr* from, int* fromlen)
{
	return 0;
}

int _select(int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds, const struct timeval* timeout)
{
	return 0;
}

int _send(SOCKET s, const char* buf, int len, int flags)
{
	return 0;
}

int _sendto(SOCKET s, const char* buf, int len, int flags, const struct sockaddr* to, int tolen)
{
	return 0;
}

int _setsockopt(SOCKET s, int level, int optname, const char* optval, int optlen)
{
	return 0;
}

int _shutdown(SOCKET s, int how)
{
	return 0;
}

SOCKET _socket(int af, int type, int protocol)
{
	return 0;
}

struct hostent* _gethostbyaddr(const char* addr, int len, int type)
{
	return 0;
}

struct hostent* _gethostbyname(const char* name)
{
	return 0;
}

int _gethostname(char* name, int namelen)
{
	return 0;
}

struct servent* _getservbyport(int port, const char* proto)
{
	return 0;
}

struct servent* _getservbyname(const char* name, const char* proto)
{
	return 0;
}

struct protoent* _getprotobynumber(int number)
{
	return 0;
}

struct protoent* _getprotobyname(const char* name)
{
	return 0;
}

#endif  /* _WIN32 */
