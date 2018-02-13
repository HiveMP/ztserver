#pragma once

#include "./defs.hpp"

void forward_packet_zt(
	struct forwarded_port* forwarded_port,
	const struct sockaddr_in* localservice,
	char* buffer,
	ssize_t_v count);

void forward_from_zerotier(
	struct forwarded_port* forwarded_port,
	const struct sockaddr_in* localservice);