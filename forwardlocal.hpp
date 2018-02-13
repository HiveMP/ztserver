#pragma once

#include "./defs.hpp"

void forward_packet(
	struct forwarded_port* forwarded_port,
	const struct sockaddr_in* localservice,
	bool src_addr_is_ipv6,
	const uint32_t* src_addr,
	uint16_t src_port,
	char* buffer,
	ssize_t_v count);

void forward_from_local(
	struct forwarded_port* forwarded_port,
	const struct sockaddr_in* localservice);