#include <stdio.h>
#include "libzt.h"
#include <inttypes.h>
#include <iostream>

#if defined(_WIN32)
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Ws2def.h>
#include <stdint.h>
#include <windows.h>
#endif

int main()
{
	uint64_t nwid = 0x8bd5124fd6206336;
	zts_startjoin("place", nwid);

	int result, socket_fd;
	fd_set readset;

	socket_fd = zts_socket(AF_INET, SOCK_STREAM, 0);
	sockaddr_in servicezt4;
	servicezt4.sin_family = AF_INET;
	servicezt4.sin_addr.s_addr = INADDR_ANY;
	servicezt4.sin_port = htons(9595);
	int err = zts_bind(socket_fd, (struct sockaddr*)&servicezt4, sizeof(sockaddr_in));
	err = zts_listen(socket_fd, 1);

	std::cout << "########################## STARTED #########################" << std::endl;

	sockaddr acc_addr;
	socklen_t acc_addr_sz = sizeof(acc_addr);
	int acc_fd = zts_accept(socket_fd, &acc_addr, &acc_addr_sz);

	std::cout << "########################## ACCEPTED #########################" << std::endl;

	while (true)
	{
		FD_ZERO(&readset);
		FD_SET(acc_fd, &readset);
		struct timeval timeout;
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;
		result = zts_select(acc_fd + 1, &readset, NULL, NULL, &timeout);
		DEBUG_INFO("result=%d", result);
		if (result > 0)
		{
			char buf[1024];
			memset(&buf, 0, sizeof(buf));
			if (zts_read(acc_fd, &buf, sizeof(buf)) <= 0) {
				break;
			}
			DEBUG_INFO("buf=%s", &buf);
		}
	}
	return 0;
}