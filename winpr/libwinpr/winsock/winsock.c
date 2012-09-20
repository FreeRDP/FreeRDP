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

#ifndef _WIN32



#endif
