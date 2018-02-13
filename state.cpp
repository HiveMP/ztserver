#include "./state.hpp"

bool zt_is_running = false;
uint64_t zt_network;
std::map<int, struct forwarded_port*>* ports;