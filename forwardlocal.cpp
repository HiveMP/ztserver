#include <cstdlib>
#if defined(_WIN32)
#include <WinSock2.h>
#include <stdint.h>
#include <WS2tcpip.h>
#endif
#include <iostream>
#include "./state.hpp"
#include "./proto.hpp"
#include "./forwardlocal.hpp"
#include "./forwardzt.hpp"

void *get_in_addr_local(struct sockaddr *sa) {
	return sa->sa_family == AF_INET
		? (void *) &(((struct sockaddr_in*)sa)->sin_addr)
		: (void *) &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void forward_packet(
	struct forwarded_port* forwarded_port,
	const struct sockaddr_in* localservice,
	bool src_addr_is_ipv6,
	const uint32_t* src_addr,
	uint16_t src_port,
	char* buffer,
	ssize_t_v count)
{
	if (!src_addr_is_ipv6)
	{
		char* sendbuf = (char*)malloc(IPV4_PREFIX_SIZE + count);
		sendbuf[0] = IPV4_TYPE;
		struct sockaddr_in* src_addr4 = (struct sockaddr_in*)src_addr;
		memcpy(sendbuf + 1, &src_addr4->sin_addr, sizeof(IN_ADDR));
		memcpy(sendbuf + 5, &src_addr4->sin_port, sizeof(USHORT));
		memcpy(sendbuf + 7, buffer, count);
		if (sendto(
			forwarded_port->socket,
			sendbuf,
			IPV4_PREFIX_SIZE + count,
			0,
			(SOCKADDR*)localservice,
			sizeof(struct sockaddr_in)
		) == SOCKET_ERROR)
		{
			wchar_t *s = NULL;
			FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
				NULL, WSAGetLastError(),
				MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
				(LPWSTR)&s, 0, NULL);
			fprintf(stderr, "%S\n", s);
			LocalFree(s);
		}
		else
		{
			std::cout << "forwarded packet from ZeroTier address " << std::endl;
		}
		free(sendbuf);
	}
	else
	{
		char* sendbuf = (char*)malloc(IPV6_PREFIX_SIZE + count);
		sendbuf[0] = IPV6_TYPE;
		struct sockaddr_in6* src_addr6 = (struct sockaddr_in6*)src_addr;
		memcpy(sendbuf + 1, &src_addr6->sin6_addr, sizeof(IN6_ADDR));
		memcpy(sendbuf + 17, &src_addr6->sin6_port, sizeof(USHORT));
		memcpy(sendbuf + 19, buffer, count);
		if (sendto(
			forwarded_port->socket,
			sendbuf,
			IPV6_PREFIX_SIZE + count,
			0,
			(SOCKADDR*)localservice,
			sizeof(struct sockaddr_in)
		) == SOCKET_ERROR)
		{
			wchar_t *s = NULL;
			FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
				NULL, WSAGetLastError(),
				MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
				(LPWSTR)&s, 0, NULL);
			fprintf(stderr, "%S\n", s);
			LocalFree(s);
		}
		else
		{
			std::cout << "forwarded packet from ZeroTier address " << std::endl;
		}
		free(sendbuf);
	}

}

void forward_from_local(
	struct forwarded_port* forwarded_port,
	const struct sockaddr_in* localservice)
{
	while (forwarded_port->running)
	{
		int result;
		fd_set readset;

		do
		{
			FD_ZERO(&readset);
			FD_SET(forwarded_port->socket, &readset);
			struct timeval timeout;
			timeout.tv_sec = 1;
			timeout.tv_usec = 0;
			result = select(forwarded_port->socket + 1, &readset, NULL, NULL, &timeout);
			if (!forwarded_port->running)
			{
				break;
			}
		} while (result == -1 && (errno == EINTR || errno == ETIME));
		if (!forwarded_port->running)
		{
			break;
		}

		if (result > 0)
		{
			if (FD_ISSET(forwarded_port->socket, &readset))
			{
				char buffer[2048];
				struct sockaddr_storage src_addr;
				socklen_t src_addr_len = sizeof(src_addr);
				ssize_t_v count = recvfrom(forwarded_port->socket, buffer, sizeof(buffer), 0, (struct sockaddr*)&src_addr, &src_addr_len);
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
					forward_packet_zt(
						forwarded_port,
						localservice,
						buffer,
						count);
				}
			}
		}
		else if (result < 0)
		{
			/* An error ocurred, just print it to stdout */
		}
	}
}