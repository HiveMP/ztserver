#include <stdio.h>
#include <libzt.h>
#define _SSIZE_T_DEFINED
extern "C" {
	#include <ulfius.h>
}
#include <sstream>
#include <iostream>
#include <inttypes.h>

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

static bool zt_is_running = false;
static uint64_t zt_network;

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

int callback_forward_udp(const struct _u_request * request, struct _u_response * response, void * user_data) {
	RETURN_FAILURE("not implemented");
}

int callback_unforward_udp(const struct _u_request * request, struct _u_response * response, void * user_data) {
	RETURN_FAILURE("not implemented");
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

void *get_in_addr(struct sockaddr *sa) {
	return sa->sa_family == AF_INET
		? (void *) &(((struct sockaddr_in*)sa)->sin_addr)
		: (void *) &(((struct sockaddr_in6*)sa)->sin6_addr);
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