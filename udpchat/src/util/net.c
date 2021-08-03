#define _POSIX_C_SOURCE 200809L
#include <sys/socket.h>

#include <stddef.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <netdb.h>
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

/* Copyright (C) 2013 - 2015, Max Lv <max.c.lv@gmail.com>
 *
 * This function is part of the shadowsocks-libev.
 *
 * shadowsocks-libev is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * shadowsocks-libev is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with shadowsocks-libev; see the file COPYING. If not, see
 * <http://www.gnu.org/licenses/>.
 */
int sockaddr_cmp(const struct sockaddr_storage* addr1,
    const struct sockaddr_storage* addr2, socklen_t len)
{
	const struct sockaddr_in* p1_in = (const struct sockaddr_in*)addr1;
	const struct sockaddr_in* p2_in = (const struct sockaddr_in*)addr2;
	const struct sockaddr_in6* p1_in6 = (const struct sockaddr_in6*)addr1;
	const struct sockaddr_in6* p2_in6 = (const struct sockaddr_in6*)addr2;
	if( p1_in->sin_family < p2_in->sin_family)
		return -1;
	if( p1_in->sin_family > p2_in->sin_family)
		return 1;
	/* compare ip4 */
	if( p1_in->sin_family == AF_INET ) {
		/* just order it, ntohs not required */
		if(p1_in->sin_port < p2_in->sin_port)
			return -1;
		if(p1_in->sin_port > p2_in->sin_port)
			return 1;
		return memcmp(&p1_in->sin_addr, &p2_in->sin_addr,
		    sizeof(struct sockaddr_in));
	} else if (p1_in6->sin6_family == AF_INET6) {
		/* just order it, ntohs not required */
		if(p1_in6->sin6_port < p2_in6->sin6_port)
			return -1;
		if(p1_in6->sin6_port > p2_in6->sin6_port)
			return 1;
		return memcmp(&p1_in6->sin6_addr, &p2_in6->sin6_addr,
		    sizeof(struct sockaddr_in6));
	} else {
		/* eek unknown type, perform this comparison for sanity. */
		return memcmp(addr1, addr2, len);
	}
}
