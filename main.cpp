#define PORT 8080

#include <libzt.h>
#define _SSIZE_T_DEFINED
extern "C" {
#include <ulfius.h>
}
#include "./state.hpp"
#include "./rest.hpp"

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