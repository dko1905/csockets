#ifndef ADDR2STR_H
#define ADDR2STR_H

#include <sys/socket.h>

#include <netinet/in.h>

/* Convert sockaddr into human readable string. The string is stored in static
 * memory, and will be overwritten on the next call. This function is thread-
 * safe. On error errno is set and NULL is returned.
 */
const char *addr2str(int _af, const struct sockaddr *_src);

#endif
