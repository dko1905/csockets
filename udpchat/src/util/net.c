#define _POSIX_C_SOURCE 200809L
#include <sys/socket.h>

#include <stddef.h>
#include <errno.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include "net.h"

/* Helper macros. */
#define MAX(_a, _b) ((_a > _b) ? _a : _b)
#define MIN(_a, _b) ((_a < _b) ? _a : _b)

/* Static buffer to hold ip, can max hold ipv6. */
#define _ip_buffer_len (MAX(INET_ADDRSTRLEN, INET6_ADDRSTRLEN))
static __thread char _ip_buffer[_ip_buffer_len] = "";

const char *addr2str(int af, const struct sockaddr *src)
{
	const struct sockaddr_in6 *addr6 = (void *)src;
	const struct sockaddr_in  *addr4 = (void *)src;

	switch (af) {
	case AF_INET:
		return inet_ntop(af, &addr4->sin_addr, _ip_buffer,
		    sizeof(_ip_buffer));
	case AF_INET6:
		return inet_ntop(af, &addr6->sin6_addr, _ip_buffer,
		    sizeof(_ip_buffer));
	default:
		errno = EAFNOSUPPORT; /* Address family not supported. */
		return NULL;
	}
}

/* Go throught all IPs provided and create one large addrinfo list.
 * If bind_ips is NULL, let the OS choose.
 */
int create_addrinfo(const char *port, const char **bind_ips,
    size_t bind_ips_len, struct addrinfo **out)
{
	struct addrinfo *first = NULL, *current = NULL, hints = {0};
	int status = 1, ret = 0;

	hints.ai_family = AF_UNSPEC; /* Any IP version. */
	hints.ai_socktype = SOCK_DGRAM; /* Must be datagram. */
	/* Host must be an ip and if no host is provided let the os choose. */
	hints.ai_flags = AI_PASSIVE | AI_NUMERICHOST;
	hints.ai_protocol = IPPROTO_UDP; /* Must be UDP. */

	for (size_t n = 0; n < bind_ips_len; ++n) {
		ret = getaddrinfo(bind_ips[n], port, &hints, &current);
		if (ret != 0) {
			if (first != NULL) {
				freeaddrinfo(first);
			}
			status = ret;
			goto getaddrinfo_err;
		}
		if (first == NULL) {
			first = current;
		} else {
			/* Set the last element of first to current. */
			struct addrinfo *tmp = first;
			while (tmp->ai_next != NULL) {
				tmp = tmp->ai_next;
			}
			tmp->ai_next = current;
		}
	}

	if (bind_ips_len == 0) {
		ret = getaddrinfo(NULL, port, &hints, &first);
		if (ret != 0) {
			status = ret;
			goto getaddrinfo_err;
		}
	}

	status = 0;
	*out = first;

getaddrinfo_err:
	return status;
}
