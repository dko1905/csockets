#define _POSIX_C_SOURCE 200809L /* POSIX-2008 */
#include <sys/types.h>
#include <sys/socket.h>

#include <stddef.h>
#include <stdbool.h>
#include <poll.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <netdb.h>

#include "util/net.h"

/* == Helper macros == */
#define MAX(_a, _b) ((_a > _b) ? _a : _b)
#define MIN(_a, _b) ((_a < _b) ? _a : _b)

/* == Compile time options == */
/* = Logging = */
/* Controls wether the log* functions print anything. */
#ifndef PRINT_DEBUG
#define PRINT_DEBUG (1)
#endif
#ifndef PRINT_INFO
#define PRINT_INFO (1)
#endif
#ifndef PRINT_WARN
#define PRINT_WARN (1)
#endif
#ifndef PRINT_ERROR
#define PRINT_ERROR (1)
#endif
#define PRINT_WRITE_MUTEX 1
#include "util/print.h"
/* = net =*/
#ifndef MAX_BIND_COUNT
/* Max count of interfaces to listen on. Normally only two are used. */
#define MAX_BIND_COUNT (10)
#endif
#ifndef MAX_MSG_SIZE
/* Max size of message that can be received. */
#define MAX_MSG_SIZE (2048)
#endif

/* == Globals == */
static char *progname = "";

/* == Static functions == */
static void *master_func(void *args);
struct master_func_args {
	int *sfd_arr;
	size_t sfd_arr_len;
};

int main(int argc, char *argv[])
{
	(void)argc; (void)argv;
	/* Use ret to check for ret errors, status is exit status. */
	int ret = 0, status = 0;
	char *bind_ips[MAX_BIND_COUNT] = {0}, *port = "";
	size_t bind_ips_len = 0;
	/* The master thread will handle all IO while main thread will sleep.*/
	pthread_t master_thread = 0;
	/* Info about sockets that we will bind to. */
	struct addrinfo *addr = NULL;
	/* Array of bound sockets. */
	int sfd_arr[MAX_BIND_COUNT] = {0};
	size_t sfd_arr_len = 0;
	/* Args to master_func. */
	struct master_func_args master_args = {0};

	/* Set program name. */
	progname = argv[0];
	/* Parse args. argc variables:
	 * 0: progname
	 * 1: port
	 * 2+: addresses to bind to
	 */
	if (argc == 2) {
		port = argv[1];
	} else if (argc > 2) {
		port = argv[1];
		for (int n = 2; n < argc; ++n) {
			if (bind_ips_len >= MAX_BIND_COUNT - 1) {
				perror("Too many interfaces provided");
				goto args_err;
			}
			bind_ips[bind_ips_len++] = argv[n];
		}
	} else {
		perror("Not enough arguments");
		goto args_err;
	}

	/* Create address info from provided infomation. */
	ret = create_addrinfo(port, (const char **)bind_ips, bind_ips_len,
	    &addr);
	if (ret != 0) {
		perror("create_addrinfo: %s", gai_strerror(ret));
		goto addrinfo_err;
	}

	/* Bind each node in addrinfo into sfd_arr. */
	for (struct addrinfo *ca = addr; ca != NULL; ca = ca->ai_next) {
		int sfd = socket(ca->ai_family, ca->ai_socktype,
		    ca->ai_protocol);
		if (sfd == -1) {
			pwarn("socket: skipping '%s': %s",
			    addr2str(ca->ai_family, ca->ai_addr),
			    strerror(errno));
			continue;
		}
		pdebug("socket: Created '%s'",
		    addr2str(ca->ai_family, ca->ai_addr));

#ifdef __linux__
		/* Disable dual stack on Linux. */
		if (ca->ai_family == AF_INET6) {
			int use_v6only = true;
			ret = setsockopt(sfd, IPPROTO_IPV6, IPV6_V6ONLY,
			    &use_v6only, sizeof(use_v6only));
			if (ret != 0) {
				pwarn("Failed to disable dualstack (linux) '%s': %s",
				    addr2str(ca->ai_family, ca->ai_addr),
				    strerror(errno));
				close(sfd);
				continue;
			}
		}
#endif
		/* Bind socket. */
		ret = bind(sfd, ca->ai_addr, ca->ai_addrlen);
		if (ret != 0) {
			pwarn("Failed to bind '%s': %s",
			    addr2str(ca->ai_family, ca->ai_addr),
			    strerror(errno));
			close(sfd);
			continue;
		}
		pdebug("Bound '%s' to %i", addr2str(ca->ai_family, ca->ai_addr),
		    sfd);

		/* Add socket to array of bound sockets. */
		if (sfd_arr_len >= MAX_BIND_COUNT) {
			pwarn("Reached socket limit (%zu) drop %s", sfd_arr_len,
			    addr2str(ca->ai_family, ca->ai_addr));
			close(sfd);
			break;
		}
		sfd_arr[sfd_arr_len++] = sfd;
	}
	if (sfd_arr_len == 0) {
		perror("No sockets creating, aborting");
		goto socket_err;
	}

	/* Start master thread. */
	master_args.sfd_arr = sfd_arr;
	master_args.sfd_arr_len = sfd_arr_len;
	ret = pthread_create(&master_thread, NULL, master_func, &master_args);
	if (ret != 0) {
		perror("Failed to create master thread: %s", strerror(ret));
		goto pthread_err;
	}

	pthread_join(master_thread, NULL);
pthread_err:
	for (size_t n = 0; n < sfd_arr_len; ++n) {
		close(sfd_arr[n]);
	}
socket_err:
	freeaddrinfo(addr);
addrinfo_err:
args_err:
	return status;
}


static void *master_func(void *args0) {
	struct master_func_args *args = (struct master_func_args *)args0;
	struct pollfd poll_arr[MAX_BIND_COUNT] = {0};
	char buffer[MAX_MSG_SIZE] = {0};

	/* Fill poll array. */
	for (size_t n = 0; n < args->sfd_arr_len && n < MAX_BIND_COUNT; ++n) {
		poll_arr[n].fd = args->sfd_arr[n];
		poll_arr[n].events = POLLIN | POLLPRI;
		/* The rest is already 0. */
	}
	for (size_t n = args->sfd_arr_len; n < MAX_BIND_COUNT; ++n) {
		poll_arr[n].fd = -1; /* poll **will** ignore under 0. */
		/* The rest is already 0. */
	}

	return NULL;
}
