#include <stdio.h>
#include <libzt.h>
#define _SSIZE_T_DEFINED
extern "C" {
	#include <ulfius.h>
}
#include <sstream>
#include <iostream>
#include <inttypes.h>
#include <map>
#include <thread>
#include <chrono>

#define PORT 8080

#define RETURN_SUCCESS \
{ \
	json_t* result = json_object(); \
	json_object_set_new(result, "outcome", json_boolean(true)); \
	ulfius_set_json_body_response(response, 200, result); \
	json_decref(result); \
	return U_CALLBACK_CONTINUE; \
}

#define RETURN_FAILURE(err) \
{ \
	json_t* result = json_object(); \
	json_object_set_new(result, "outcome", json_boolean(false)); \
	json_object_set_new(result, "error", json_string(err)); \
	ulfius_set_json_body_response(response, 200, result); \
	json_decref(result); \
	return U_CALLBACK_CONTINUE; \
}

struct forwarded_port {
	int proxyport;
	int localport;
	int remoteport;
	SOCKET socket;
	int ztsocket4;
	int ztsocket6;
	bool running;
	std::thread* thread_ztinbound;
	std::thread* thread_localinbound;
};

static bool zt_is_running = false;
static uint64_t zt_network;
static std::map<int, struct forwarded_port*>* ports;

void *get_in_addr(struct sockaddr *sa) {
	return sa->sa_family == AF_INET
		? (void *) &(((struct sockaddr_in*)sa)->sin_addr)
		: (void *) &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int callback_join(const struct _u_request * request, struct _u_response * response, void * user_data) {
	json_t* req = ulfius_get_json_body_request(request, NULL);

	if (!json_is_object(req)) {
		RETURN_FAILURE("expected json object as request body");
	}

	json_t* js_nwid = json_object_get(req, "nwid");
	if (js_nwid == nullptr) {
		RETURN_FAILURE("request json does not have 'nwid' key");
	}
	if (!json_is_string(js_nwid)) {
		RETURN_FAILURE("request json 'nwid' value is not string");
	}

	json_t* js_path = json_object_get(req, "path");
	if (js_path == nullptr) {
		RETURN_FAILURE("request json does not have 'path' key");
	}
	if (!json_is_string(js_path)) {
		RETURN_FAILURE("request json 'path' value is not string");
	}

	const char* nwid_str = json_string_value(js_nwid);
	std::string nwid_cppstr(nwid_str);

	uint64_t nwid;
	std::istringstream iss(nwid_cppstr);
	iss >> std::hex >> nwid;

	if (zt_is_running) {
		RETURN_FAILURE("zerotier network has already been joined");
	}

	if (zts_startjoin(json_string_value(js_path), nwid) == 0)
	{
		zt_is_running = true;
		zt_network = nwid;
		RETURN_SUCCESS;
	}
	else
	{
		RETURN_FAILURE("unable to initialise zerotier");
	}
}

#define IPV4_PREFIX_SIZE (1 + 4 + 2)
#define IPV6_PREFIX_SIZE (1 + 16 + 2)
#define IPV4_TYPE 0
#define IPV6_TYPE 1

void forward_packet(
	struct forwarded_port* forwarded_port,
	const struct sockaddr_in* localservice,
	struct sockaddr_storage* src_addr, 
	char* buffer, 
	ssize_t count)
{
	if (src_addr->ss_family == AF_INET)
	{
		char* sendbuf = (char*)malloc(IPV4_PREFIX_SIZE + count);
		sendbuf[0] = IPV4_TYPE;
		struct sockaddr_in* src_addr4 = (struct sockaddr_in*)src_addr;
		memcpy(sendbuf + 1, &src_addr4->sin_addr, sizeof(IN_ADDR));
		memcpy(sendbuf + 5, &src_addr4->sin_port, sizeof(USHORT));
		memcpy(sendbuf + 7, buffer, count);
		sendto(
			forwarded_port->socket,
			sendbuf,
			IPV4_PREFIX_SIZE + count,
			0,
			(SOCKADDR*)&localservice,
			sizeof(struct sockaddr_in)
		);
		free(sendbuf);
	}
	else if (src_addr->ss_family == AF_INET6)
	{
		char* sendbuf = (char*)malloc(IPV6_PREFIX_SIZE + count);
		sendbuf[0] = IPV6_TYPE;
		struct sockaddr_in6* src_addr6 = (struct sockaddr_in6*)src_addr;
		memcpy(sendbuf + 1, &src_addr6->sin6_addr, sizeof(IN6_ADDR));
		memcpy(sendbuf + 17, &src_addr6->sin6_port, sizeof(USHORT));
		memcpy(sendbuf + 19, buffer, count);
		sendto(
			forwarded_port->socket,
			sendbuf,
			IPV6_PREFIX_SIZE + count,
			0,
			(SOCKADDR*)&localservice,
			sizeof(struct sockaddr_in)
		);
		free(sendbuf);
	}

	char* buf2 = (char*)malloc(4096);
	memset(buf2, 0, 4096);
	inet_ntop(src_addr->ss_family, get_in_addr(
		(struct sockaddr *)src_addr), buf2, 4095);
	std::cout << "forwarded packet from ZeroTier address " << buf2 << std::endl;
	free(buf2);
}

void forward_packet_zt(
	struct forwarded_port* forwarded_port,
	const struct sockaddr_in* localservice,
	char* buffer,
	ssize_t count)
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
		memcpy(buffer + 1, &src_addr4.sin_addr, 4);
		memcpy(buffer + 5, &src_addr4.sin_port, 2);

		char* buf2 = (char*)malloc(4096);
		memset(buf2, 0, 4096);
		inet_ntop(src_addr4.sin_family, get_in_addr(
			(struct sockaddr *)&src_addr4), buf2, 4095);
		std::cout << "forwarded packet to ZeroTier address " << buf2 << std::endl;
		free(buf2);

		zts_sendto(
			forwarded_port->ztsocket4,
			buffer + IPV4_PREFIX_SIZE,
			count - IPV4_PREFIX_SIZE,
			0,
			(SOCKADDR*)&src_addr4,
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
		inet_ntop(src_addr6.sin6_family, get_in_addr(
			(struct sockaddr *)&src_addr6), buf2, 4095);
		std::cout << "forwarded packet to ZeroTier address " << buf2 << std::endl;
		free(buf2);

		zts_sendto(
			forwarded_port->ztsocket4,
			buffer + IPV6_PREFIX_SIZE,
			count - IPV6_PREFIX_SIZE,
			0,
			(SOCKADDR*)&src_addr6,
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
			if (FD_ISSET(&readset, forwarded_port->ztsocket4))
			{
				char buffer[2048];
				struct sockaddr_storage src_addr;
				socklen_t src_addr_len = sizeof(src_addr);
				ssize_t count = zts_recvfrom(forwarded_port->ztsocket4, buffer, sizeof(buffer), 0, (struct sockaddr*)&src_addr, &src_addr_len);
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
						&src_addr,
						buffer,
						count);
				}
			}

			if (FD_ISSET(&readset, forwarded_port->ztsocket6))
			{
				char buffer[2048];
				struct sockaddr_storage src_addr;
				socklen_t src_addr_len = sizeof(src_addr);
				ssize_t count = zts_recvfrom(forwarded_port->ztsocket6, buffer, sizeof(buffer), 0, (struct sockaddr*)&src_addr, &src_addr_len);
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
						&src_addr,
						buffer,
						count);
				}
			}
		}
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
				ssize_t count = recvfrom(forwarded_port->socket, buffer, sizeof(buffer), 0, (struct sockaddr*)&src_addr, &src_addr_len);
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

int callback_forward_udp(const struct _u_request * request, struct _u_response * response, void * user_data)
{
	if (!zt_is_running) 
	{
		RETURN_FAILURE("zerotier network has not been joined");
	}

	json_t* req = ulfius_get_json_body_request(request, NULL);

	if (!json_is_object(req)) 
	{
		RETURN_FAILURE("expected json object as request body");
	}

	int real_localport;
	json_t* js_localport = json_object_get(req, "localport");
	if (js_localport == nullptr) 
	{
		RETURN_FAILURE("request json does not have 'localport' key");
	}
	if (!json_is_integer(js_localport))
	{
		RETURN_FAILURE("request json 'localport' value is not integer");
	}
	else
	{
		real_localport = json_integer_value(js_localport);
	}

	int real_proxyport;
	json_t* js_proxyport = json_object_get(req, "proxyport");
	if (js_proxyport != nullptr)
	{
		if (!json_is_integer(js_proxyport))
		{
			RETURN_FAILURE("request json 'proxyport' value is not integer");
		}
		else
		{
			real_proxyport = json_integer_value(js_proxyport);
		}
	}
	else
	{
		real_proxyport = 0;
	}

	sockaddr_in localservice;
	localservice.sin_family = AF_INET;
	localservice.sin_addr.s_addr = inet_addr("127.0.0.1");
	localservice.sin_port = htons(real_localport);

	struct forwarded_port* forwarded_port = new struct forwarded_port();

	// Create the local UDP socket.
	forwarded_port->proxyport = 0;
	forwarded_port->localport = 0;
	forwarded_port->remoteport = 0;
	forwarded_port->running = true;
	forwarded_port->thread_ztinbound = nullptr;
	forwarded_port->thread_localinbound = nullptr;
	forwarded_port->socket = socket(AF_INET, SOCK_DGRAM, 0);
	forwarded_port->ztsocket4 = -1;
	forwarded_port->ztsocket6 = -1;
	if (forwarded_port->socket == INVALID_SOCKET) {
		delete forwarded_port;
		RETURN_FAILURE("unable to create UDP socket");
	}
	sockaddr_in service;
	service.sin_family = AF_INET;
	service.sin_addr.s_addr = inet_addr("127.0.0.1");
	if (real_proxyport > 0 && real_proxyport < 65536) {
		service.sin_port = htons(real_proxyport);
	}
	if (bind(forwarded_port->socket, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR) {
		closesocket(forwarded_port->socket);
		delete forwarded_port;
		RETURN_FAILURE("unable to bind to localhost");
	}
	int assigned_addr_len = sizeof(service);
	if (getsockname(forwarded_port->socket, (SOCKADDR*)&service, &assigned_addr_len) == SOCKET_ERROR) {
		closesocket(forwarded_port->socket);
		delete forwarded_port;
		RETURN_FAILURE("unable to get the assigned port of listening socket");
	}
	forwarded_port->proxyport = ntohs(service.sin_port);

	// Create the ZeroTier socket.
	std::cout << "creating zerotier socket..." << std::endl;
	forwarded_port->ztsocket4 = zts_socket(AF_INET, SOCK_DGRAM, 0);
	if (forwarded_port->ztsocket4 == INVALID_SOCKET)
	{
		closesocket(forwarded_port->socket);
		delete forwarded_port;
		RETURN_FAILURE("unable to create ZT UDP4 socket");
	}
	forwarded_port->ztsocket6 = zts_socket(AF_INET6, SOCK_DGRAM, 0);
	if (forwarded_port->ztsocket6 == INVALID_SOCKET)
	{
		zts_close(forwarded_port->ztsocket4);
		closesocket(forwarded_port->socket);
		delete forwarded_port;
		RETURN_FAILURE("unable to create ZT UDP6 socket");
	}
	struct sockaddr_storage servicezt4_stor;
	struct sockaddr_in* servicezt4 = (struct sockaddr_in*)&servicezt4_stor;
	servicezt4->sin_family = AF_INET;
	servicezt4->sin_addr.s_addr = INADDR_ANY;
	servicezt4->sin_port = 0;
	if (zts_get_address(zt_network, &servicezt4_stor, AF_INET) == -1)
	{
		zts_close(forwarded_port->ztsocket4);
		zts_close(forwarded_port->ztsocket6);
		closesocket(forwarded_port->socket);
		delete forwarded_port;
		RETURN_FAILURE("unable to get the ZeroTier IPv4 interface");
	}
	servicezt4->sin_port = 0;
	if (zts_bind(forwarded_port->ztsocket4, (SOCKADDR*)servicezt4, sizeof(sockaddr_in)) != 0)
	{
		zts_close(forwarded_port->ztsocket4);
		zts_close(forwarded_port->ztsocket6);
		closesocket(forwarded_port->socket);
		delete forwarded_port;
		RETURN_FAILURE("unable to bind to ZeroTier IPv4 address");
	}
	int assigned_zt_addr_len = sizeof(servicezt4);
	if (zts_getsockname(forwarded_port->ztsocket4, (SOCKADDR*)servicezt4, &assigned_zt_addr_len) == SOCKET_ERROR)
	{
		zts_close(forwarded_port->ztsocket4);
		zts_close(forwarded_port->ztsocket6);
		closesocket(forwarded_port->socket);
		delete forwarded_port;
		RETURN_FAILURE("unable to get the assigned port of ZeroTier socket");
	}
	sockaddr_in6 servicezt6;
	servicezt6.sin6_family = AF_INET6;
	servicezt6.sin6_addr = IN6ADDR_ANY_INIT;
	servicezt6.sin6_port = 0;
	if (zts_bind(forwarded_port->ztsocket6, (SOCKADDR*)&servicezt6, sizeof(sockaddr_in6)) != 0)
	{
		zts_close(forwarded_port->ztsocket4);
		zts_close(forwarded_port->ztsocket6);
		closesocket(forwarded_port->socket);
		delete forwarded_port;
		RETURN_FAILURE("unable to bind to ZeroTier IPv6 address");
	}
	int assigned_zt_addr6_len = sizeof(servicezt6);
	if (zts_getsockname(forwarded_port->ztsocket6, (SOCKADDR*)&servicezt6, &assigned_zt_addr6_len) == SOCKET_ERROR)
	{
		zts_close(forwarded_port->ztsocket4);
		zts_close(forwarded_port->ztsocket6);
		closesocket(forwarded_port->socket);
		delete forwarded_port;
		RETURN_FAILURE("unable to get the assigned port of ZeroTier socket");
	}

	// Create the thread that forwards packets from ZeroTier IPv4 and IPv6 to the local service.
	forwarded_port->thread_ztinbound = new std::thread([forwarded_port, localservice]()
	{
		forward_from_zerotier(
			forwarded_port,
			&localservice
		);
	});

	// Create the thread that forwards local traffic to the ZeroTier network.
	forwarded_port->thread_localinbound = new std::thread([forwarded_port, localservice]()
	{
		forward_from_local(
			forwarded_port,
			&localservice
		);
	});

	// Append to the map.
	ports->insert(std::pair<int, struct forwarded_port*>(service.sin_port, forwarded_port));

	json_t* result = json_object();
	json_object_set_new(result, "localport", json_integer(ntohs(localservice.sin_port)));
	json_object_set_new(result, "proxyport", json_integer(ntohs(service.sin_port)));
	json_object_set_new(result, "zt4port", json_integer(ntohs(servicezt4->sin_port)));
	json_object_set_new(result, "zt6port", json_integer(ntohs(servicezt6.sin6_port)));
	ulfius_set_json_body_response(response, 200, result);
	json_decref(result);

	return U_CALLBACK_CONTINUE;
}

int callback_unforward_udp(const struct _u_request * request, struct _u_response * response, void * user_data) {
	json_t* req = ulfius_get_json_body_request(request, NULL);

	if (!json_is_object(req)) {
		RETURN_FAILURE("expected json object as request body");
	}

	json_t* js_port = json_object_get(req, "localport");
	if (js_port == nullptr) {
		RETURN_FAILURE("request json does not have 'localport' key");
	}
	if (!json_is_integer(js_port)) {
		RETURN_FAILURE("request json 'localport' value is not integer");
	}

	auto existing_port = ports->find(json_integer_value(js_port));
	if (existing_port == ports->end()) {
		RETURN_FAILURE("no such forwarded port");
	}

	// Tell the thread to stop.
	existing_port->second->running = false;
	
	// Join the threads until they are actually finished.
	existing_port->second->thread_ztinbound->join();
	existing_port->second->thread_localinbound->join();

	// Delete the thread and mapped value.
	auto temp = existing_port->second;
	ports->erase(existing_port);
	delete temp->thread_ztinbound;
	delete temp->thread_localinbound;
	delete temp;

	RETURN_SUCCESS;
}

int callback_shutdown(const struct _u_request * request, struct _u_response * response, void * user_data) {
	if (!zt_is_running) {
		RETURN_FAILURE("zerotier network has not been joined");
	}

	zts_stop();
	zt_is_running = false;
	zt_network = 0;
	RETURN_SUCCESS;
}

int callback_get_info(const struct _u_request * request, struct _u_response * response, void * user_data) {
	if (!zt_is_running) {
		RETURN_FAILURE("zerotier network has not been joined");
	}

	char* homepath = (char*)malloc(4096);
	memset(homepath, 0, 4096);
	zts_get_path(homepath, 4095);

	char* buf = (char*)malloc(4096);
	memset(buf, 0, 4096);
	sprintf(buf, "%" PRIx64, zts_get_node_id());

	char* nwidbuf = (char*)malloc(4096);
	memset(nwidbuf, 0, 4096);
	sprintf(nwidbuf, "%" PRIx64, zt_network);

	json_t* addressarray = json_array();
	for (int i = 0; i < zts_get_num_assigned_addresses(zt_network); i++) {
		struct sockaddr_storage addr;
		zts_get_address_at_index(zt_network, i, &addr);
		char* buf2 = (char*)malloc(4096);
		memset(buf2, 0, 4096);
		inet_ntop(addr.ss_family, get_in_addr(
			(struct sockaddr *)&addr), buf2, 4095);
		json_array_append_new(addressarray, json_string(buf2));
		free(buf2);
	}

	json_t* result = json_object();
	json_object_set_new(result, "path", json_string(homepath));
	json_object_set_new(result, "nwid", json_string(nwidbuf));
	json_object_set_new(result, "nodeid", json_string(buf));
	json_object_set_new(result, "addresses", addressarray);
	ulfius_set_json_body_response(response, 200, result);
	json_decref(result);

	free(homepath);
	free(buf);
	free(nwidbuf);

	return U_CALLBACK_CONTINUE;
}

int main(void) {
	struct _u_instance instance;

	ports = new std::map<int, struct forwarded_port*>();

	// Set port to 9995 to avoid conflicts with real ZeroTier service.
	zts_set_service_port(9995);

	// Initialize instance with the port number
	if (ulfius_init_instance(&instance, PORT, NULL, NULL) != U_OK) {
		fprintf(stderr, "Error ulfius_init_instance, abort\n");
		return(1);
	}

	// Endpoint list declaration
	ulfius_add_endpoint_by_val(&instance, "PUT", "/join", NULL, 0, &callback_join, NULL);
	ulfius_add_endpoint_by_val(&instance, "PUT", "/forward", NULL, 0, &callback_forward_udp, NULL);
	ulfius_add_endpoint_by_val(&instance, "PUT", "/unforward", NULL, 0, &callback_unforward_udp, NULL);
	ulfius_add_endpoint_by_val(&instance, "PUT", "/shutdown", NULL, 0, &callback_shutdown, NULL);
	ulfius_add_endpoint_by_val(&instance, "GET", "/info", NULL, 0, &callback_get_info, NULL);

	// Start the framework
	if (ulfius_start_framework(&instance) == U_OK) {
		printf("Start framework on port %d\n", instance.port);

		// Wait for the user to press <enter> on the console to quit the application
		getchar();
	}
	else {
		fprintf(stderr, "Error starting framework\n");
	}
	printf("End framework\n");

	ulfius_stop_framework(&instance);
	ulfius_clean_instance(&instance);

	return 0;
}