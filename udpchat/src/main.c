#define _POSIX_C_SOURCE 200809L /* POSIX-2008 */
#include <sys/types.h>
#include <sys/socket.h>

#include <stddef.h>
#include <stdbool.h>
#include <poll.h>
#include <pthread.h>

#include <netdb.h>

#include "util/net.h"

/* == Helper macros == */
#define MAX(_a, _b) ((_a > _b) ? _a : _b)
#define MIN(_a, _b) ((_a < _b) ? _a : _b)

/* == Compile time options == */
/* = Logging = */
/* Controls wether the log* functions print anything. */
#ifndef LOG_DEBUG
#define LOG_DEBUG (1)
#endif
#ifndef LOG_INFO
#define LOG_INFO (1)
#endif
#ifndef LOG_WARN
#define LOG_WARN (1)
#endif
#ifndef LOG_ERROR
#define LOG_ERROR (1)
#endif
#include "util/logging.h"
/* = net =*/
#ifndef MAX_BIND_COUNT
/* Max count of interfaces to listen on. Normally only two are used. */
#define MAX_BIND_COUNT (10)
#endif

/* == Globals == */
static char *progname = "";

/* == Static functions == */

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
	struct addrinfo *bindinfo = NULL;

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
				log_error("Too many interfaces provided");
				goto args_err;
			}
			bind_ips[bind_ips_len++] = argv[n];
		}
	} else {
		log_error("Not enough arguments");
		goto args_err;
	}

	ret = create_addrinfo(port, (const char **)bind_ips, bind_ips_len,
	    &bindinfo);
	if (ret != 0) {
		log_error("create_addrinfo: %s", gai_strerror(ret));
		goto addrinfo_err;
	}


	freeaddrinfo(bindinfo);
addrinfo_err:
args_err:
	return status;
}

