/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Server UDP listener: platform-specific socket code
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

/* Required for struct in6_pktinfo before system headers */
#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE // NOLINT(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp)
#endif
#if defined(__APPLE__) && !defined(__APPLE_USE_RFC_3542)
#define __APPLE_USE_RFC_3542 // NOLINT(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp)
#endif

#include <errno.h>
#include <string.h>

#include <winpr/assert.h>
#include <winpr/cast.h>
#include <winpr/winsock.h>

/* From mstcpip.h; fails to compile with NONAMELESSUNION */
#if defined(_WIN32) && !defined(SIO_UDP_CONNRESET)
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif
#if defined(_WIN32) && !defined(SIO_UDP_NETRESET)
#define SIO_UDP_NETRESET _WSAIOW(IOC_VENDOR, 15)
#endif

#if !defined(_WIN32)
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <netinet/in.h>
#endif

/* Sockets bound to the unspecified address can determine the local address used for
 * each datagram, allowing replies to be sent from the address the client contacted. */
#if !defined(_WIN32) && defined(IP_PKTINFO)
#define UDPLISTENER_LOCAL_ADDRESS_IPV4
#endif
#if !defined(_WIN32) && defined(IPV6_RECVPKTINFO) && defined(IPV6_PKTINFO)
#define UDPLISTENER_LOCAL_ADDRESS_IPV6
#endif

#if defined(_WIN32) && !defined(WSA_FLAG_NO_HANDLE_INHERIT)
#define WSA_FLAG_NO_HANDLE_INHERIT 0x80
#endif

#include <freerdp/log.h>

#include "udplistener_internal.h"

#define TAG FREERDP_TAG("core.udplistener")

WINPR_ATTR_NODISCARD
BOOL udpsocket_address_equal(const struct sockaddr_storage* a, const struct sockaddr* b,
                             size_t blen)
{
	WINPR_ASSERT(a);
	WINPR_ASSERT(b);
	if ((blen < sizeof(b->sa_family)) || (a->ss_family != b->sa_family))
		return FALSE;
	if (a->ss_family == AF_INET)
	{
		const struct sockaddr_in* x = (const struct sockaddr_in*)a;
		const struct sockaddr_in* y = (const struct sockaddr_in*)b;
		return (blen >= sizeof(*y)) && (x->sin_port == y->sin_port) &&
		       (memcmp(&x->sin_addr, &y->sin_addr, sizeof(x->sin_addr)) == 0);
	}
	if (a->ss_family == AF_INET6)
	{
		const struct sockaddr_in6* x = (const struct sockaddr_in6*)a;
		const struct sockaddr_in6* y = (const struct sockaddr_in6*)b;
		/* Link-local addresses on different interfaces are distinct. */
		return (blen >= sizeof(*y)) && (x->sin6_port == y->sin6_port) &&
		       (x->sin6_scope_id == y->sin6_scope_id) &&
		       (memcmp(&x->sin6_addr, &y->sin6_addr, sizeof(x->sin6_addr)) == 0);
	}
	return FALSE;
}

void udpsocket_unmap_address(const struct sockaddr* addr, size_t addrlen,
                             struct sockaddr_storage* out, size_t* outlen)
{
	WINPR_ASSERT(addr);
	WINPR_ASSERT(addrlen <= sizeof(*out));

	memcpy(out, addr, addrlen);
	*outlen = addrlen;
	if ((addr->sa_family != AF_INET6) || (addrlen < sizeof(struct sockaddr_in6)))
		return;
	const struct sockaddr_in6* v6 = (const struct sockaddr_in6*)addr;
	if (!IN6_IS_ADDR_V4MAPPED(&v6->sin6_addr))
		return;

	struct sockaddr_in v4 = WINPR_C_ARRAY_INIT;
	v4.sin_family = AF_INET;
	v4.sin_port = v6->sin6_port;
	memcpy(&v4.sin_addr, &v6->sin6_addr.s6_addr[12], sizeof(v4.sin_addr));
	memset(out, 0, sizeof(*out));
	memcpy(out, &v4, sizeof(v4));
	*outlen = sizeof(v4);
}

/* Prevent listener sockets from leaking into exec'd child processes; children that
 * fork without exec'ing retain them (see udp.h). Use SOCK_CLOEXEC where available
 * to ensure atomicity and prevent race conditions with other threads calling exec(). */
void udpsocket_set_no_inherit(SOCKET sockfd)
{
#if defined(_WIN32)
	/* May fail under a layered service provider; socket remains usable. */
	if (!SetHandleInformation((HANDLE)sockfd, HANDLE_FLAG_INHERIT, 0))
		WLog_WARN(TAG, "unable to prevent child processes from acquiring a socket");
#else
	const int flags = fcntl((int)sockfd, F_GETFD);
	if ((flags < 0) || (fcntl((int)sockfd, F_SETFD, flags | FD_CLOEXEC) != 0))
		WLog_WARN(TAG, "failed to set close-on-exec for the socket");
#endif
}

/* Use the no-inherit flag during creation where supported to prevent race conditions
 * with other threads starting processes. */
SOCKET udpsocket_new(int family, int type, int protocol)
{
#if defined(_WIN32)
	SOCKET sockfd = WSASocketW(family, type, protocol, nullptr, 0,
	                           WSA_FLAG_OVERLAPPED | WSA_FLAG_NO_HANDLE_INHERIT);
	/* Windows 7 without SP does not support this flag. */
	if ((sockfd == INVALID_SOCKET) && (WSAGetLastError() == WSAEINVAL))
		sockfd = WSASocketW(family, type, protocol, nullptr, 0, WSA_FLAG_OVERLAPPED);
#else
	SOCKET sockfd = _socket(family, type | UDPLISTENER_SOCK_CLOEXEC, protocol);
#endif
	if (sockfd != INVALID_SOCKET)
		udpsocket_set_no_inherit(sockfd);
	return sockfd;
}

SOCKET udpsocket_bind(const struct sockaddr* addr, size_t addrlen)
{
	WINPR_ASSERT(addr);

	SOCKET sockfd = udpsocket_new(addr->sa_family, SOCK_DGRAM, IPPROTO_UDP);
	if (sockfd == INVALID_SOCKET)
		return INVALID_SOCKET;

	const int on = 1;
	if ((addr->sa_family == AF_INET6) &&
	    (_setsockopt(sockfd, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&on, sizeof(on)) != 0))
		WLog_WARN(TAG, "failed to set IPV6_V6ONLY via setsockopt()");
#if defined(_WIN32)
	/* Prevent other processes under the same user from binding to the port and
	 * intercepting datagrams. */
	if (_setsockopt(sockfd, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char*)&on, sizeof(on)) != 0)
	{
		closesocket(sockfd);
		return INVALID_SOCKET;
	}
#endif
	/* Avoid SO_REUSEADDR to prevent multiple sockets from competing for datagrams, as
	 * there is no mechanism to route them across processes. Multi-process servers will
	 * fail here and fallback to TCP unless relaying is used (see udp.h). */
	/* Smaller buffers may increase retransmissions; see RDPUDP_RECEIVE_BUFFER. */
	const UINT32 rcvbuf = UDPLISTENER_RECEIVE_BUFFER;
	if (_setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (const char*)&rcvbuf, sizeof(rcvbuf)) != 0)
		WLog_WARN(TAG, "failed to set SO_RCVBUF via setsockopt()");

	if (_bind(sockfd, addr, WINPR_ASSERTING_INT_CAST(int, addrlen)) != 0)
	{
		/* e.g. port already in use; clients will fallback to TCP. */
		WLog_WARN(TAG, "failed to bind UDP socket for RDP-UDP, error %d", WSAGetLastError());
		closesocket(sockfd);
		return INVALID_SOCKET;
	}
	return sockfd;
}

void udpsocket_ignore_icmp_errors(SOCKET sockfd)
{
#if defined(_WIN32)
	/* Prevent ICMP "port unreachable" messages from causing subsequent receives to fail. */
	BOOL report = FALSE;
	DWORD bytes = 0;
	if (WSAIoctl(sockfd, SIO_UDP_CONNRESET, &report, sizeof(report), nullptr, 0, &bytes, nullptr,
	             nullptr) != 0)
		WLog_DBG(TAG, "unable to disable SIO_UDP_CONNRESET");
	/* Same for ICMP "time exceeded". */
	if (WSAIoctl(sockfd, SIO_UDP_NETRESET, &report, sizeof(report), nullptr, 0, &bytes, nullptr,
	             nullptr) != 0)
		WLog_DBG(TAG, "unable to disable SIO_UDP_NETRESET");
#else
	WINPR_UNUSED(sockfd);
#endif
}

BOOL udpsocket_is_message_socket(SOCKET sockfd)
{
	int type = 0;
	int typeLength = sizeof(type);
	return (_getsockopt(sockfd, SOL_SOCKET, SO_TYPE, (char*)&type, &typeLength) == 0) &&
	       ((type == SOCK_SEQPACKET) || (type == SOCK_DGRAM));
}

WINPR_ATTR_NODISCARD
static BOOL udpsocket_is_any_address(const struct sockaddr_storage* addr)
{
	WINPR_ASSERT(addr);
	if (addr->ss_family == AF_INET)
	{
		const struct sockaddr_in* in = (const struct sockaddr_in*)addr;
		return in->sin_addr.s_addr == htonl(INADDR_ANY);
	}
	if (addr->ss_family == AF_INET6)
	{
		const struct sockaddr_in6* in6 = (const struct sockaddr_in6*)addr;
		return IN6_IS_ADDR_UNSPECIFIED(&in6->sin6_addr);
	}
	return FALSE;
}

void udpsocket_enable_local_address(udpListenerSocket* sock)
{
	WINPR_ASSERT(sock);

	if (!udpsocket_is_any_address(&sock->addr))
		return;

	int rc = -1;
#if defined(UDPLISTENER_LOCAL_ADDRESS_IPV4) || defined(UDPLISTENER_LOCAL_ADDRESS_IPV6)
	const int on = 1;
#endif
#if defined(UDPLISTENER_LOCAL_ADDRESS_IPV4)
	if (sock->addr.ss_family == AF_INET)
		rc = _setsockopt(sock->sockfd, IPPROTO_IP, IP_PKTINFO, (const char*)&on, sizeof(on));
#endif
#if defined(UDPLISTENER_LOCAL_ADDRESS_IPV6)
	if (sock->addr.ss_family == AF_INET6)
		rc =
		    _setsockopt(sock->sockfd, IPPROTO_IPV6, IPV6_RECVPKTINFO, (const char*)&on, sizeof(on));
#endif
	sock->localAddress = (rc == 0);
	if (!sock->localAddress)
		WLog_WARN(TAG,
		          "a socket bound to any address cannot determine the destination of a datagram. "
		          "With more than one address, a reply can go out from an incorrect address");
}

int udpsocket_receive(const udpListenerSocket* sock, BYTE* data, size_t size,
                      struct sockaddr_storage* addr, int* addrlen, udpLocalAddress* local)
{
	WINPR_ASSERT(sock);
	WINPR_ASSERT(data);
	WINPR_ASSERT(addr);
	WINPR_ASSERT(addrlen);
	WINPR_ASSERT(local);

	memset(local, 0, sizeof(*local));
#if defined(UDPLISTENER_LOCAL_ADDRESS_IPV4) || defined(UDPLISTENER_LOCAL_ADDRESS_IPV6)
	if (sock->localAddress)
	{
		union
		{
			struct cmsghdr header;
			BYTE data[256];
		} control;
		memset(&control, 0, sizeof(control));
		struct iovec iov = { .iov_base = data, .iov_len = size };
		struct msghdr msg = WINPR_C_ARRAY_INIT;
		msg.msg_name = addr;
		msg.msg_namelen = WINPR_ASSERTING_INT_CAST(socklen_t, *addrlen);
		msg.msg_iov = &iov;
		msg.msg_iovlen = 1;
		msg.msg_control = control.data;
		msg.msg_controllen = sizeof(control.data);
		const ssize_t got = recvmsg(WINPR_ASSERTING_INT_CAST(int, sock->sockfd), &msg, 0);
		if (got < 0)
			return -1;
		*addrlen = WINPR_ASSERTING_INT_CAST(int, msg.msg_namelen);

		for (struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg))
		{
#if defined(UDPLISTENER_LOCAL_ADDRESS_IPV4)
			if ((cmsg->cmsg_level == IPPROTO_IP) && (cmsg->cmsg_type == IP_PKTINFO) &&
			    (cmsg->cmsg_len >= CMSG_LEN(sizeof(struct in_pktinfo))))
			{
				struct in_pktinfo info = WINPR_C_ARRAY_INIT;
				memcpy(&info, CMSG_DATA(cmsg), sizeof(info));
				local->family = AF_INET;
				local->ipv4 = info.ipi_addr;
				local->interfaceIndex = WINPR_ASSERTING_INT_CAST(UINT32, info.ipi_ifindex);
				local->valid = TRUE;
			}
#endif
#if defined(UDPLISTENER_LOCAL_ADDRESS_IPV6)
			if ((cmsg->cmsg_level == IPPROTO_IPV6) && (cmsg->cmsg_type == IPV6_PKTINFO) &&
			    (cmsg->cmsg_len >= CMSG_LEN(sizeof(struct in6_pktinfo))))
			{
				struct in6_pktinfo info = WINPR_C_ARRAY_INIT;
				memcpy(&info, CMSG_DATA(cmsg), sizeof(info));
				local->family = AF_INET6;
				local->ipv6 = info.ipi6_addr;
				local->interfaceIndex = WINPR_ASSERTING_INT_CAST(UINT32, info.ipi6_ifindex);
				local->valid = TRUE;
			}
#endif
		}
		return WINPR_ASSERTING_INT_CAST(int, got);
	}
#endif
	return _recvfrom(sock->sockfd, (char*)data, WINPR_ASSERTING_INT_CAST(int, size), 0,
	                 (struct sockaddr*)addr, addrlen);
}

BOOL udpsocket_send_from(SOCKET sockfd, const struct sockaddr_storage* to, size_t tolen,
                         const udpLocalAddress* local, const BYTE* data, size_t length)
{
	WINPR_ASSERT(to);
	WINPR_ASSERT(local);
	WINPR_ASSERT(data);

#if defined(UDPLISTENER_LOCAL_ADDRESS_IPV4) || defined(UDPLISTENER_LOCAL_ADDRESS_IPV6)
	union
	{
		struct cmsghdr header;
		BYTE data[256];
	} control;
	memset(&control, 0, sizeof(control));
	struct iovec iov = { .iov_base = WINPR_CAST_CONST_PTR_AWAY(data, void*), .iov_len = length };
	struct msghdr msg = WINPR_C_ARRAY_INIT;
	msg.msg_name = WINPR_CAST_CONST_PTR_AWAY(to, void*);
	msg.msg_namelen = WINPR_ASSERTING_INT_CAST(socklen_t, tolen);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = control.data;
	msg.msg_controllen = sizeof(control.data);
	struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
	if (!cmsg)
	{
		errno = EINVAL; /* Caller lets the system pick the address */
		return FALSE;
	}

	BOOL known = FALSE;
#if defined(UDPLISTENER_LOCAL_ADDRESS_IPV4)
	if (local->family == AF_INET)
	{
		/* The route picks the interface */
		struct in_pktinfo info = WINPR_C_ARRAY_INIT;
		info.ipi_spec_dst = local->ipv4;
		cmsg->cmsg_level = IPPROTO_IP;
		cmsg->cmsg_type = IP_PKTINFO;
		cmsg->cmsg_len = CMSG_LEN(sizeof(info));
		memcpy(CMSG_DATA(cmsg), &info, sizeof(info));
		msg.msg_controllen = CMSG_SPACE(sizeof(info));
		known = TRUE;
	}
#endif
#if defined(UDPLISTENER_LOCAL_ADDRESS_IPV6)
	if (local->family == AF_INET6)
	{
		/* Link-local addresses are interface-specific. For others, the route picks the interface,
		 * as interfaces may disappear and return with a new index. */
		struct in6_pktinfo info = WINPR_C_ARRAY_INIT;
		info.ipi6_addr = local->ipv6;
		if (IN6_IS_ADDR_LINKLOCAL(&local->ipv6))
			info.ipi6_ifindex = local->interfaceIndex;
		cmsg->cmsg_level = IPPROTO_IPV6;
		cmsg->cmsg_type = IPV6_PKTINFO;
		cmsg->cmsg_len = CMSG_LEN(sizeof(info));
		memcpy(CMSG_DATA(cmsg), &info, sizeof(info));
		msg.msg_controllen = CMSG_SPACE(sizeof(info));
		known = TRUE;
	}
#endif
	if (!known)
	{
		errno = EINVAL; /* Caller lets the system pick the address */
		return FALSE;
	}
	return sendmsg(WINPR_ASSERTING_INT_CAST(int, sockfd), &msg, 0) >= 0;
#else
	WINPR_UNUSED(sockfd);
	WINPR_UNUSED(tolen);
	WINPR_UNUSED(length);
	return FALSE;
#endif
}

BOOL udpsocket_source_address_unavailable(int* error)
{
	WINPR_ASSERT(error);
#if !defined(_WIN32)
	/* Interface for the address is gone */
	const int systemError = errno;
	const BOOL gone = (systemError == ENODEV) || (systemError == ENXIO);
#else
	const BOOL gone = FALSE;
#endif
	*error = WSAGetLastError();
	return gone || (*error == WSAEADDRNOTAVAIL) || (*error == WSAEINVAL);
}

BOOL udpsocket_peer_closed(void)
{
	/* SOCK_DGRAM pairs don't report peer closure; only sends fail */
#if defined(_WIN32)
	const int error = WSAGetLastError();
	return (error == WSAECONNRESET) || (error == WSAECONNREFUSED) || (error == WSAENOTCONN);
#else
	const int error = errno;
	return (error == ECONNREFUSED) || (error == ENOTCONN) || (error == EPIPE) ||
	       (error == ECONNRESET);
#endif
}

BOOL udpsocket_message_pair(SOCKET pair[2])
{
	WINPR_ASSERT(pair);

	pair[0] = INVALID_SOCKET;
	pair[1] = INVALID_SOCKET;
#if defined(_WIN32)
	/* Winsock lacks socketpair(); use two loopback sockets that other processes
	 * cannot share */
	struct sockaddr_in addr[2] = WINPR_C_ARRAY_INIT;
	for (size_t x = 0; x < 2; x++)
	{
		int length = sizeof(addr[x]);
		addr[x].sin_family = AF_INET;
		addr[x].sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		pair[x] = udpsocket_new(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		/* Prevent other processes from binding the same port to intercept traffic */
		const BOOL exclusive = TRUE;
		if ((pair[x] == INVALID_SOCKET) ||
		    (_setsockopt(pair[x], SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char*)&exclusive,
		                 sizeof(exclusive)) != 0) ||
		    (_bind(pair[x], (const struct sockaddr*)&addr[x], sizeof(addr[x])) != 0) ||
		    (_getsockname(pair[x], (struct sockaddr*)&addr[x], &length) != 0))
			goto fail;
	}
	for (size_t x = 0; x < 2; x++)
	{
		if (_connect(pair[x], (const struct sockaddr*)&addr[1 - x], sizeof(addr[1 - x])) != 0)
			goto fail;
	}
	/* Drain any datagrams sent by other processes before connect() */
	for (size_t x = 0; x < 2; x++)
	{
		u_long nonBlocking = 1;
		if (_ioctlsocket(pair[x], FIONBIO, &nonBlocking) != 0)
			goto fail;
		char drop[64] = WINPR_C_ARRAY_INIT;
		while ((_recv(pair[x], drop, sizeof(drop), 0) >= 0) || (WSAGetLastError() == WSAEMSGSIZE))
		{
		}
		nonBlocking = 0;
		if (_ioctlsocket(pair[x], FIONBIO, &nonBlocking) != 0)
			goto fail;
	}
#else
	/* Some systems (e.g. macOS) lack AF_UNIX SOCK_SEQPACKET. SOCK_DGRAM also preserves
	 * boundaries; it doesn't report peer closure, but both ends send an empty
	 * message before closing. */
	int sv[2] = { -1, -1 };
	if ((socketpair(AF_UNIX, SOCK_SEQPACKET | UDPLISTENER_SOCK_CLOEXEC, 0, sv) != 0) &&
	    (socketpair(AF_UNIX, SOCK_DGRAM | UDPLISTENER_SOCK_CLOEXEC, 0, sv) != 0))
		return FALSE;
	pair[0] = (SOCKET)sv[0];
	pair[1] = (SOCKET)sv[1];
	udpsocket_set_no_inherit(pair[0]);
	udpsocket_set_no_inherit(pair[1]);
#endif

	return TRUE;

#if defined(_WIN32)
fail:
	for (size_t x = 0; x < 2; x++)
	{
		if (pair[x] != INVALID_SOCKET)
			closesocket(pair[x]);
		pair[x] = INVALID_SOCKET;
	}
	return FALSE;
#endif
}

WINPR_ATTR_NODISCARD
BOOL freerdp_udp_socket_pair(int fds[2])
{
	WINPR_ASSERT(fds);

	SOCKET pair[2] = { INVALID_SOCKET, INVALID_SOCKET };
	if (!udpsocket_message_pair(pair))
		return FALSE;

	/* All client data passes through here; small buffers only increase resends */
	const UINT32 size = UDPLISTENER_RECEIVE_BUFFER;
	for (size_t x = 0; x < 2; x++)
	{
		if ((_setsockopt(pair[x], SOL_SOCKET, SO_SNDBUF, (const char*)&size, sizeof(size)) != 0) ||
		    (_setsockopt(pair[x], SOL_SOCKET, SO_RCVBUF, (const char*)&size, sizeof(size)) != 0))
			WLog_DBG(TAG, "failed to set the buffer sizes of a socket pair");
		fds[x] = (int)pair[x];
	}
	return TRUE;
}

void udpsocket_poll_set(udpPollFd* fd, SOCKET sockfd)
{
	WINPR_ASSERT(fd);
#if defined(_WIN32)
	fd->fd = sockfd;
#else
	fd->fd = WINPR_ASSERTING_INT_CAST(int, sockfd);
#endif
	fd->events = POLLIN;
	fd->revents = 0;
}

int udpsocket_poll(udpPollFd* fds, size_t count)
{
#if defined(_WIN32)
	return WSAPoll(fds, WINPR_ASSERTING_INT_CAST(ULONG, count), -1);
#else
	return poll(fds, WINPR_ASSERTING_INT_CAST(nfds_t, count), -1);
#endif
}
