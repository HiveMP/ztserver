#include <stdio.h>
#include <ulfius.h>

#include <libzt.h>

#define PORT 8080

int callback_hello_world(const struct _u_request * request, struct _u_response * response, void * user_data) {
	ulfius_set_string_body_response(response, 200, "Hello World!");
	return U_CALLBACK_CONTINUE;
}

int main(void) {
	struct _u_instance instance;

	// Initialize instance with the port number
	if (ulfius_init_instance(&instance, PORT, NULL, NULL) != U_OK) {
		fprintf(stderr, "Error ulfius_init_instance, abort\n");
		return(1);
	}

	// Endpoint list declaration
	ulfius_add_endpoint_by_val(&instance, "GET", "/helloworld", NULL, 0, &callback_hello_world, NULL);

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