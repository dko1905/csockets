#include <stdint.h> // For uint16_t
#include <poll.h> // For pollfd struct

#ifndef SOCKETLOOP_H
#define SOCKETLOOP_H

#define MAXBACKLOG 10
#define POLLMAX 20

void socketloop(uint16_t port, int timeout, int verbose);
int create_socket(uint16_t port, int verbose);
void clienthandle(struct pollfd *fds, int verbose);

#endif