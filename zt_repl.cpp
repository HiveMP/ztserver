#include <stdio.h>
#include "libzt.h"
#include <inttypes.h>
#include <thread>
#include <iostream>

#if defined(_WIN32)
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Ws2def.h>
#include <stdint.h>
#include <windows.h>
#endif

// Whether to listen and select on a UDP IPv4 socket.
// #define USE_IPV4

// Whether to listen and select on a UDP IPv6 socket.
// #define USE_IPV6

// Whether the UDP IPv4 socket should be FD 1 instead of FD 0.
// #define IPV4_AS_SECOND_SOCKET

// Whether to use a multi-threaded setup.
// #define USE_THREADS

// Whether to disable zts_select and zts_recvfrom calls for FD 0.
// #define NO_FD0_SELECT

// Whether to disable zts_select and zts_recvfrom calls for FD 1.
// #define NO_FD1_SELECT

int main()
{
	uint64_t nwid = 0x8bd5124fd6206336;
	zts_startjoin("place", nwid);

	int result, socket_fd4, socket_fd6, err;

#if !defined(USE_THREADS)
	fd_set readset;
#endif

#if defined(USE_IPV4) && (!defined(USE_IPV6) || !defined(IPV4_AS_SECOND_SOCKET))
	socket_fd4 = zts_socket(AF_INET, SOCK_DGRAM, 0);
	sockaddr_in servicezt4;
	servicezt4.sin_family = AF_INET;
	servicezt4.sin_addr.s_addr = INADDR_ANY;
	servicezt4.sin_port = htons(9595);
	err = zts_bind(socket_fd4, (struct sockaddr*)&servicezt4, sizeof(sockaddr_in));
	err = zts_listen(socket_fd4, 1);
#endif

#if defined(USE_IPV6)
	socket_fd6 = zts_socket(AF_INET6, SOCK_DGRAM, 0);
	sockaddr_in6 servicezt6;
	servicezt6.sin6_family = AF_INET6;
	servicezt6.sin6_addr = IN6ADDR_ANY_INIT;
	servicezt6.sin6_port = htons(9595);
	err = zts_bind(socket_fd6, (struct sockaddr*)&servicezt6, sizeof(sockaddr_in6));
	err = zts_listen(socket_fd6, 1);
#endif

#if defined(USE_IPV4) && defined(USE_IPV6) && defined(IPV4_AS_SECOND_SOCKET)
	socket_fd4 = zts_socket(AF_INET, SOCK_DGRAM, 0);
	sockaddr_in servicezt4;
	servicezt4.sin_family = AF_INET;
	servicezt4.sin_addr.s_addr = INADDR_ANY;
	servicezt4.sin_port = htons(9595);
	err = zts_bind(socket_fd4, (struct sockaddr*)&servicezt4, sizeof(sockaddr_in));
	err = zts_listen(socket_fd4, 1);
#endif

	std::cout << "--------------------------------------------------------------------------" << std::endl;
	std::cout << "!!!!!!!!!!!!!!!!!!!! WE HAVE STARTED !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << std::endl;
	std::cout << "--------------------------------------------------------------------------" << std::endl;

#if defined(USE_IPV4)
#if defined(USE_THREADS)
	auto t4 = std::thread([socket_fd4]()
	{
		fd_set readset;
		int result;
#endif
		while (true)
		{
#if ((!defined(NO_FD0_SELECT) && !defined(IPV4_AS_SECOND_SOCKET)) || (!defined(NO_FD1_SELECT) && defined(IPV4_AS_SECOND_SOCKET)))
			FD_ZERO(&readset);
			FD_SET(socket_fd4, &readset);
			struct timeval timeout;
			timeout.tv_sec = 0;
			timeout.tv_usec = 100;
			result = zts_select(socket_fd4 + 1, &readset, NULL, NULL, &timeout);
			DEBUG_INFO("result=%d", result);
			if (result > 0)
			{
				char buffer[2048];
				struct sockaddr_storage src_addr;
				socklen_t src_addr_len = sizeof(src_addr);
				ssize_t count = zts_recvfrom(socket_fd4, buffer, sizeof(buffer), 0, (struct sockaddr*)&src_addr, &src_addr_len);
				DEBUG_INFO("buf=%s", &buf);
			}
#elif defined(USE_THREADS)
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
#endif
#if !(defined(USE_IPV4) && defined(USE_IPV6) && !defined(USE_THREADS))
		}
#endif
#if defined(USE_THREADS)
	});
#endif
#endif

#if defined(USE_IPV6)
#if defined(USE_THREADS)
	auto t6 = std::thread([socket_fd6]()
	{
		fd_set readset;
		int result;
#endif
#if !(defined(USE_IPV4) && defined(USE_IPV6) && !defined(USE_THREADS))
		while (true)
		{
#endif
#if ((!defined(NO_FD1_SELECT) && !defined(IPV4_AS_SECOND_SOCKET)) || (!defined(NO_FD0_SELECT) && defined(IPV4_AS_SECOND_SOCKET)))
			FD_ZERO(&readset);
			FD_SET(socket_fd6, &readset);
#if (!defined(USE_THREADS) && ( \
		!(defined(USE_IPV4) && defined(USE_IPV6)) || \
		!((!defined(NO_FD0_SELECT) && !defined(IPV4_AS_SECOND_SOCKET)) || (!defined(NO_FD1_SELECT) && defined(IPV4_AS_SECOND_SOCKET))) \
	)) || defined(USE_THREADS)
			struct timeval timeout;
#endif
			timeout.tv_sec = 0;
			timeout.tv_usec = 100;
			result = zts_select(socket_fd6 + 1, &readset, NULL, NULL, &timeout);
			DEBUG_INFO("result=%d", result);
			if (result > 0)
			{
				char buffer[2048];
				struct sockaddr_storage src_addr;
				socklen_t src_addr_len = sizeof(src_addr);
				ssize_t count = zts_recvfrom(socket_fd6, buffer, sizeof(buffer), 0, (struct sockaddr*)&src_addr, &src_addr_len);
				DEBUG_INFO("buf=%s", &buf);
			}
#elif defined(USE_THREADS)
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
#endif
		}
#if defined(USE_THREADS)
	});
#endif
#endif
#if defined(USE_THREADS)
#if defined(USE_IPV6)
	t6.join();
#elif defined(USE_IPV4)
	t4.join();
#endif
#endif

	return 0;
}