#include <sys/socket.h>

#include <stddef.h>
#include <errno.h>
#include <threads.h>

#include <netinet/in.h>
#include <arpa/inet.h>

/* Helper macros. */
#define MAX(_a, _b) ((_a > _b) ? _a : _b)
#define MIN(_a, _b) ((_a < _b) ? _a : _b)

/* Static buffer to hold ip, can max hold ipv6. */
#define _ip_buffer_len (MAX(INET_ADDRSTRLEN, INET6_ADDRSTRLEN))
static thread_local char _ip_buffer[_ip_buffer_len] = "";

const char *addr2str(int af, const struct sockaddr *src)
{
	struct sockaddr_in6 addr6 = {0};
	struct sockaddr_in  addr4 = {0};

	switch (af) {
	case AF_INET:
		return inet_ntop(af, &addr4.sin_addr, _ip_buffer,
		    sizeof(_ip_buffer));
	case AF_INET6:
		return inet_ntop(af, &addr6.sin6_addr, _ip_buffer,
		    sizeof(_ip_buffer));
	default:
		errno = EAFNOSUPPORT; /* Address family not supported. */
		return NULL;
	}
}
