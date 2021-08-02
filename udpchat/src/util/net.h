#ifndef UTIL_NET_H
#define UTIL_NET_H
#include <sys/socket.h> /* sockaddr */
#include <netdb.h> /* addrinfo */

/* Convert sockaddr into human readable string. The string is stored in static
 * memory, and will be overwritten on the next call. This function is thread-
 * safe. On error errno is set and NULL is returned.
 *
 * Possible errors:
 * EAFNOSUPPORT - Address family not supported!
 */
const char *addr2str(int _af, const struct sockaddr *_src);
/* Create addrinfo from supplied info. If bind_ips is NULL, the OS chooses
 * which addresses to bind to. On error getaddrinfo is returned, else 0.
 *
 * NOTES:
 * Please use gai_strerror to get human readable error.
 */
int create_addrinfo(const char *_port, const char **_bind_ips,
    size_t _bind_ips_len, struct addrinfo **_addr);

#endif /* UTIL_NET_H */
