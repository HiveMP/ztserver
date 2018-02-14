#include <cstdlib>
#define LWIP_IPV6 1
#define LWIP_SOCKET 1
#include <lwip/sockets.h>
#include <iostream>
#include "./state.hpp"
#include "./proto.hpp"
#include "./forwardlocal.hpp"
#include "./forwardzt.hpp"

extern "C"
{
#if defined(__MING32__) || defined(__MING64__)
#ifdef ADD_EXPORTS
#define ZT_SOCKET_API __declspec(dllexport)
#else
#define ZT_SOCKET_API __declspec(dllimport)
#endif
#define ZTCALL __cdecl
#else
#define ZT_SOCKET_API
#define ZTCALL
#endif

	ZT_SOCKET_API int ZTCALL zts_select(
		int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
	ZT_SOCKET_API ssize_t_v ZTCALL zts_sendto(
		int fd, const void *buf, size_t len, int flags, const struct sockaddr *addr, socklen_t addrlen);
	ZT_SOCKET_API ssize_t_v ZTCALL zts_recvfrom(
		int fd, void *buf, size_t len, int flags, struct sockaddr *addr, socklen_t *addrlen);
}

void *get_in_addr_zt(struct sockaddr *sa) {
	return sa->sa_family == AF_INET
		? (void *) &(((struct sockaddr_in*)sa)->sin_addr)
		: (void *) &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void forward_packet_zt(
	struct forwarded_port* forwarded_port,
	unsigned char* buffer,
	ssize_t_v count)
{
	if (count < 1)
	{
		return;
	}

	if (buffer[0] == IPV4_TYPE)
	{
		if (count < IPV4_PREFIX_SIZE)
		{
			return;
		}

		struct sockaddr_in src_addr4;
		src_addr4.sin_family = AF_INET;
		memcpy(&src_addr4.sin_addr, buffer + 1, 4);
		memcpy(&src_addr4.sin_port, buffer + 5, 2);

		char* buf2 = (char*)malloc(4096);
		memset(buf2, 0, 4096);
		inet_ntop(src_addr4.sin_family, get_in_addr_zt(
			(struct sockaddr *)&src_addr4), buf2, 4095);
		std::cout << "forwarded packet to ZeroTier address " << buf2 << std::endl;
		free(buf2);

		zts_sendto(
			forwarded_port->ztsocket4,
			buffer + IPV4_PREFIX_SIZE,
			count - IPV4_PREFIX_SIZE,
			0,
			(const sockaddr*)&src_addr4,
			sizeof(struct sockaddr_in)
		);
	}
	else if (buffer[0] == IPV6_TYPE)
	{
		if (count < IPV6_PREFIX_SIZE)
		{
			return;
		}

		struct sockaddr_in6 src_addr6;
		src_addr6.sin6_family = AF_INET6;
		memcpy(buffer + 1, &src_addr6.sin6_addr, 16);
		memcpy(buffer + 17, &src_addr6.sin6_port, 2);

		char* buf2 = (char*)malloc(4096);
		memset(buf2, 0, 4096);
		inet_ntop(src_addr6.sin6_family, get_in_addr_zt(
			(struct sockaddr *)&src_addr6), buf2, 4095);
		std::cout << "forwarded packet to ZeroTier address " << buf2 << std::endl;
		free(buf2);

		zts_sendto(
			forwarded_port->ztsocket4,
			buffer + IPV6_PREFIX_SIZE,
			count - IPV6_PREFIX_SIZE,
			0,
			(struct sockaddr*)&src_addr6,
			sizeof(struct sockaddr_in6)
		);
	}
}

void forward_from_zerotier(
	struct forwarded_port* forwarded_port,
	const struct sockaddr_in* localservice)
{
	while (forwarded_port->running)
	{
		int result;
		fd_set readset;

		FD_ZERO(&readset);
		FD_SET(forwarded_port->ztsocket4, &readset);
		FD_SET(forwarded_port->ztsocket6, &readset);
		result = zts_select(
			forwarded_port->ztsocket6 + 1,
			&readset,
			NULL,
			NULL,
			NULL);
		if (!forwarded_port->running)
		{
			break;
		}

		if (result > 0)
		{
			if (FD_ISSET(forwarded_port->ztsocket4, &readset))
			{
				char buffer[2048];
				struct sockaddr_storage src_addr;
				socklen_t src_addr_len = sizeof(src_addr);
				ssize_t_v count = zts_recvfrom(forwarded_port->ztsocket4, buffer, sizeof(buffer), 0, (struct sockaddr*)&src_addr, &src_addr_len);
				if (count == -1)
				{
					// ignore packet
				}
				else if (count == sizeof(buffer))
				{
					// ignore packet
				}
				else
				{
					forward_packet(
						forwarded_port,
						localservice,
						src_addr.ss_family == AF_INET6,
						&(((const struct sockaddr_in*)&src_addr)->sin_addr).s_addr,
						((const struct sockaddr_in*)&src_addr)->sin_port,
						buffer,
						count);
				}
			}

			if (FD_ISSET(forwarded_port->ztsocket6, &readset))
			{
				char buffer[2048];
				struct sockaddr_storage src_addr;
				socklen_t src_addr_len = sizeof(src_addr);
				ssize_t_v count = zts_recvfrom(forwarded_port->ztsocket6, buffer, sizeof(buffer), 0, (struct sockaddr*)&src_addr, &src_addr_len);
				if (count == -1)
				{
					forwarded_port->running = false;
					break;
				}
				else if (count == sizeof(buffer))
				{
					continue;
				}
				else
				{
					forward_packet(
						forwarded_port,
						localservice,
						src_addr.ss_family == AF_INET6,
						(const uint32_t*)&(((const struct sockaddr_in6*)&src_addr)->sin6_addr),
						((const struct sockaddr_in6*)&src_addr)->sin6_port,
						buffer,
						count);
				}
			}
		}
	}
}