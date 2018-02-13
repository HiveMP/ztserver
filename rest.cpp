#include <libzt.h>
#include <iostream>
#include <sstream>
#define _SSIZE_T_DEFINED
extern "C" {
#include <ulfius.h>
}
#include <jansson.h>
#include "./state.hpp"
#include "./proto.hpp"
#include "./forwardzt.hpp"
#include "./forwardlocal.hpp"
#include <cinttypes>

void *get_in_addr_rest(struct sockaddr *sa) {
	return sa->sa_family == AF_INET
		? (void *) &(((struct sockaddr_in*)sa)->sin_addr)
		: (void *) &(((struct sockaddr_in6*)sa)->sin6_addr);
}

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
	forwarded_port->socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
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
	else {
		service.sin_port = 0;
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
		inet_ntop(addr.ss_family, get_in_addr_rest(
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