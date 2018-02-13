#pragma once

int callback_join(const struct _u_request * request, struct _u_response * response, void * user_data);
int callback_forward_udp(const struct _u_request * request, struct _u_response * response, void * user_data);
int callback_unforward_udp(const struct _u_request * request, struct _u_response * response, void * user_data);
int callback_shutdown(const struct _u_request * request, struct _u_response * response, void * user_data);
int callback_get_info(const struct _u_request * request, struct _u_response * response, void * user_data);
