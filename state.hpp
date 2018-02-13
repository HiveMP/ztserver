#pragma once

#include <cstdint>
#include <map>
#include <thread>
#include <basetsd.h>

struct forwarded_port {
	int proxyport;
	int localport;
	int remoteport;
	UINT_PTR socket;
	int ztsocket4;
	int ztsocket6;
	bool running;
	std::thread* thread_ztinbound;
	std::thread* thread_localinbound;
};

extern bool zt_is_running;
extern uint64_t zt_network;
extern std::map<int, struct forwarded_port*>* ports;
