#include <cxxopts.hpp>
#include <libzt.h>
#define _SSIZE_T_DEFINED
extern "C" {
#include <ulfius.h>
}
#include "./state.hpp"
#include "./rest.hpp"

bool instance_inited = false;
struct _u_instance instance;

#ifdef _WIN32
BOOL WINAPI consoleHandler(DWORD signal) 
{
	if (signal == CTRL_C_EVENT)
	{
		std::cout << "Shutting down ztserver" << std::endl;
		if (instance_inited)
		{
			ulfius_stop_framework(&instance);
			ulfius_clean_instance(&instance);
		}

		exit(0);
	}

	return TRUE;
}
#endif

int main(int argc, char* argv[])
{
#ifdef _WIN32
	if (!SetConsoleCtrlHandler(consoleHandler, TRUE)) {
		printf("\nERROR: Could not set control handler");
		return 1;
	}
#endif

	cxxopts::Options options("ztserver", "A local ZeroTier daemon for forwarding UDP traffic over ZeroTier, controllable via REST API.");
	options.add_options()
		("help", "Print help")
		("h,http-port", "The port to use for the HTTP server (default: 8080)", cxxopts::value<int>())
		("z,zt-port", "The port to use for the ZeroTier service port (default: 9995)", cxxopts::value<int>())
		;
	options.parse(argc, argv);

	if (options.count("help"))
	{
		std::cout << options.help({ "", "Group" }) << std::endl;
		return 0;
	}

	if (options.count("z"))
	{
		zts_set_service_port(options["z"].as<int>());
	}
	else
	{
		// Set port to 9995 to avoid conflicts with real ZeroTier service.
		zts_set_service_port(9995);
	}

	ports = new std::map<int, struct forwarded_port*>();

	// Initialize instance with the port number
	int http_port = 8080;
	if (options.count("h"))
	{
		http_port = options["h"].as<int>();
	}

	if (ulfius_init_instance(&instance, http_port, NULL, NULL) != U_OK) {
		fprintf(stderr, "Error ulfius_init_instance, abort\n");
		return 1;
	}

	instance_inited = true;

	// Endpoint list declaration
	ulfius_add_endpoint_by_val(&instance, "PUT", "/join", NULL, 0, &callback_join, NULL);
	ulfius_add_endpoint_by_val(&instance, "PUT", "/forward", NULL, 0, &callback_forward_udp, NULL);
	ulfius_add_endpoint_by_val(&instance, "PUT", "/unforward", NULL, 0, &callback_unforward_udp, NULL);
	ulfius_add_endpoint_by_val(&instance, "PUT", "/shutdown", NULL, 0, &callback_shutdown, NULL);
	ulfius_add_endpoint_by_val(&instance, "GET", "/info", NULL, 0, &callback_get_info, NULL);

	// Start the framework
	if (ulfius_start_framework(&instance) != U_OK)
	{
		std::cout << "Unable to start HTTP REST API on port " << instance.port << std::endl;
		instance_inited = false;
		ulfius_stop_framework(&instance);
		ulfius_clean_instance(&instance);
		return 1;
	}
	else
	{
		std::cout << "REST API listening on port " << instance.port << std::endl;
	}

	std::cout << "ztserver is now running, send SIGTERM (Ctrl-C) to exit" << std::endl;

	// Sleep forever until Ctrl-C is pressed and handled.
	while (true)
	{
		std::this_thread::sleep_for(std::chrono::hours(24));
	}

	return 0;
}