#include <sys/types.h>
#include <sys/socket.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdbool.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <stdatomic.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

#ifdef __STDC_NO_THREADS__
#define thread_local __thread
#else
#include <threads.h>
#endif

/* Helper macros. */
#define MAX(_a, _b) ((_a > _b) ? _a : _b)
#define MIN(_a, _b) ((_a < _b) ? _a : _b)

/* Max amount of addresses to try binding to. This is used when traversing
 * the addrinfo struct.
 */
#define MAX_BIND_COUNT (10)
/* Setup read timeout to 5 seconds. */
#define MAX_RECV_TIMEOUT (5)
/* The max size of datagram. */
#define MAX_DATAGRAM_SIZE (2048)
/* Max number of active users. */
#define MAX_ACTIVE_USER_COUNT (2)
/* Max size of message to send to all active users. */
#define MAX_MSG_SIZE (120)

/* TODO: write comment for chat_user. */
struct chat_user {
	struct sockaddr_storage addr;
	/* Messages sent in 60 second interval, NOT in the last 60 seconds. */
	int messages_in60;
	/* Messages sent in 5 second interval, NOT in the last 5 seconds. */
	int messages_in5;
};
static struct chat_user active_users[MAX_ACTIVE_USER_COUNT] = {0};
static size_t active_users_len = 0;
static pthread_mutex_t active_users_mutex = {0};

/* Mutex that is locked and unlocked when using logging functions. */
static pthread_mutex_t log_mutex = {0};

/* Program name, used in logging functions. */
static char *progname = "";

/* Various logging functions. */
bool pdebug_enabled = true;
bool pwarn_enabled = true;
bool perr_enabled = true;

static inline void pdebug(const char *format, ...);
static inline void pwarn(const char *format, ...);
static inline void perr(const char *format, ...);

/* Create addrinfo from supplied info. If bind_ips is NULL, the OS chooses
 * an address to bind to.
 */
static int create_addrinfo(const char *port,
    const char *bind_ips[MAX_BIND_COUNT], size_t bind_ips_len,
    struct addrinfo **addr);
/* Convert addrinfo struct to ip, supports both ipv4 and ipv6.
 * The returned string is allocated in static memory.
 */
static const char *addr2str(const struct addrinfo *addr);
static thread_local char _ip_buffer[MAX(INET6_ADDRSTRLEN, INET_ADDRSTRLEN)] = {0};

/* The main read and write loop, used in new threads. */
static void *rw_loop_func(void *args);
struct rw_loop_args {
	int sfd;
	atomic_bool run;
};

int main(int argc, char *argv[])
{
	struct addrinfo *addr = NULL;
	char *port = "";
	char *bind_ips[MAX_BIND_COUNT] = {0};
	size_t bind_ips_len = 0;
	int status = EXIT_FAILURE, ret = 0;
	int sfd_arr[MAX_BIND_COUNT] = {0};
	size_t sfd_arr_len = 0;
	pthread_t children[MAX_BIND_COUNT] = {0};
	size_t children_len = 0;
	struct rw_loop_args children_args[MAX_BIND_COUNT] = {0};
	size_t children_args_len = 0;
	sigset_t sigset = {0};

	/* Setup pthread mutex. */
	ret = pthread_mutex_init(&log_mutex, NULL);
	if (ret != 0) {
		perr("Failed to create log_mutex: %s", strerror(errno));
		goto mutex_err;
	}
	ret = pthread_mutex_init(&active_users_mutex, NULL);
	if (ret != 0) {
		perr("Failed to create active user mutex: %s", strerror(errno));
		pthread_mutex_destroy(&log_mutex);
		goto mutex_err;
	}
	/* Setup sigset to catch SIGTERM. */
	ret = sigemptyset(&sigset);
	if (ret != 0) {
		perr("Failed to setup sigset: %s", strerror(errno));
		goto sigset_err;
	}
	ret = sigaddset(&sigset, SIGTERM);
	ret = sigaddset(&sigset, SIGINT);
	if (ret != 0) {
		perr("Failed to add last signal to sigset: %s", strerror(errno));
		goto sigset_err;
	}
	ret = sigprocmask(SIG_BLOCK, &sigset, NULL);
	if (ret != 0) {
		perr("Failed to set procmask: %s", strerror(errno));
		goto sigset_err;
	}

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
				perr("Too many interfaces provided");
				goto args_err;
			}
			bind_ips[bind_ips_len++] = argv[n];
		}
	} else {
		perr("Not enough arguments");
		goto args_err;
	}

	/* Create addrinfo from supplied info. */
	ret = create_addrinfo(port, (const char **)bind_ips, bind_ips_len,
	    &addr);
	if (ret != 0) {
		perr("Failed to create addrinfo: Please check port and ip");
		goto addrinfo_err;
	}

	/* Fill fd arr with -1. */
	for (size_t n = 0; n < MAX_BIND_COUNT; ++n) {
		sfd_arr[n] = -1;
	}
	/* Create sockets and then bind them. */
	for (struct addrinfo *ca = addr; ca != NULL; ca = ca->ai_next) {
		/* Create socket. */
		int sfd = socket(ca->ai_family, ca->ai_socktype,
		    ca->ai_protocol);
		if (sfd == -1) {
			pwarn("Failed to create socket (%s): %s",
			    addr2str(ca),
			    strerror(errno));
			continue;
		}
		pdebug("Created socket %i", sfd, addr2str(ca));

		/* Bind socket. */
		ret = bind(sfd, ca->ai_addr, ca->ai_addrlen);
		if (ret != 0) {
			pwarn("Failed to bind '%s' to %i: %s", addr2str(ca),
			    sfd, strerror(errno));
			close(sfd);
			continue;
		}
		pdebug("Bound '%s' to %i", addr2str(ca), sfd);

		/* Setup timeout on socket. */
		struct timeval tv = {0};
		tv.tv_sec = MAX_RECV_TIMEOUT;
		ret = setsockopt(sfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
		if (ret != 0) {
			pwarn("Failed to set socket timeout: %s",
			    strerror(errno));
			close(sfd);
			continue;
		}

		/* Add to array of sockets. */
		if (sfd_arr_len >= MAX_BIND_COUNT) {
			pwarn("Reached socket limit (%zu)", sfd_arr_len);
			close(sfd);
			break;
		}
		sfd_arr[sfd_arr_len++] = sfd;
	}
	if (sfd_arr_len == 0) {
		perr("No sockets created, aborting");
		goto socket_err;
	}

	/* Create one thread per fd. */
	for (size_t n = 0; n < sfd_arr_len; ++n) {
		pthread_t thread = 0;

		children_args[n].sfd = sfd_arr[n];
		children_args[n].run = true;
		ret = pthread_create(&thread, NULL, rw_loop_func,
		    &children_args[n]);
		if (ret != 0) {
			perr("Failed to create thread for %i: %s", sfd_arr[n],
			    strerror(errno));
			goto thread_err;
		}

		if (children_len >= MAX_BIND_COUNT) {
			pwarn("Thread limit reached (%zu)", children_len);
			break;
		}
		children[children_len++] = thread;
	}

	/* This "hack" forces the main thread to sleep until it unlocks.*/
	ret = sigwait(&sigset, &ret);
	if (ret != 0) {
		pwarn("sigwait failed: %s", strerror(errno));
	} else {
		pdebug("sigwait: closing program nicely");
	}
	status = 0;

thread_err:
	/* Tell all thread to stop. */
	for (size_t n = 0; n < children_len; ++n) {
		atomic_store(&children_args[n].run, false);
	}
	/* Join all threads, and wait for them to stop. */
	for (size_t n = 0; n < children_len; ++n) {
		ret = pthread_join(children[n], NULL);
		if (ret != 0) {
			perr("Error from pthread_join: %s", strerror(errno));
		}
	}
	/* Close all open sockets/fds. */
	for (size_t n = 0; n < sfd_arr_len; ++n) {
		/* Socket 0,1,2 are used by stdin, stdout and stderr. */
		if (sfd_arr[n] != -1) {
			close(sfd_arr[n]);
		}
	}
socket_err:
	freeaddrinfo(addr);
addrinfo_err:
sigset_err:
	pthread_mutex_destroy(&active_users_mutex);
	pthread_mutex_destroy(&log_mutex);
mutex_err:
args_err:
	return status;
}

static void *rw_loop_func(void *args0)
{
	struct rw_loop_args *args = args0;
	char buffer[MAX_DATAGRAM_SIZE] = {0};
	struct sockaddr_storage sockaddr = {0};
	struct addrinfo addr = {0};
	ssize_t ret = 0, bytes = 0;

	addr.ai_addr = (struct sockaddr*)&sockaddr;

	pdebug("s%i: Started master loop", args->sfd);

	while (atomic_load(&args->run) == true) {
		/* Set correct size before calling. */
		addr.ai_addrlen = sizeof(sockaddr);
		/* Reset ip to 0. */
		memset(addr.ai_addr, 0, sizeof(sockaddr));

		/* Receive message and store sender ip in addr.ai_addr. */
		ret = recvfrom(args->sfd, buffer, sizeof(buffer), 0,
		    addr.ai_addr, &addr.ai_addrlen);

		/* Set family according to size of struct. */
		addr.ai_family = addr.ai_addrlen == sizeof(struct sockaddr_in) ?
		    AF_INET : AF_INET6;
		/* Print sender ip and bytes read. */
		if (ret < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				pdebug("s%i: TIMEOUT", args->sfd,
				    addr2str(&addr), ret);
				continue;
			}
			perr("Encountered error from recvfrom: %s",
			     strerror(errno));
			return NULL+1;
		} else {
			bytes = ret;
			pdebug("s%i: %s: %d bytes", args->sfd, addr2str(&addr),
			    bytes);
		}

		/* TODO: throtale (I know it's spelled wrong). */

		/* Send message to all active users. */
		for (size_t n = 0; n < active_users_len; ++n) {
			ret = sendto(args->sfd, buffer, 
			    MIN((size_t)bytes, ), )
		}

		/* Send the message back. */
		ret = sendto(args->sfd, buffer, (size_t)bytes, 0, addr.ai_addr,
		    addr.ai_addrlen);
		if (ret < 0) {
			perr("Encountered error from sendto: %s",
			    strerror(errno));
			return NULL+1;
		} else if (ret < bytes) {
			pwarn("Could not send full message (%zu of %zu): %s",
			    (size_t)ret, (size_t)bytes, strerror(errno));
		}
	}

	return NULL;
}

static const char *addr2str(const struct addrinfo *addr)
{
	if (addr->ai_family == AF_INET) {
		struct sockaddr_in *a4 = (struct sockaddr_in *)addr->ai_addr;
		const char *ret = inet_ntop(AF_INET, (void *)&a4->sin_addr,
		    _ip_buffer, sizeof(_ip_buffer));
		if (ret == NULL) {
			perr("Failed to convert addrinfo4 to str: %s",
			    strerror(errno));
			return NULL;
		}
		return ret;
	} else if(addr->ai_family == AF_INET6) {
		struct sockaddr_in6 *a6 = (struct sockaddr_in6 *)addr->ai_addr;
		const char *ret = inet_ntop(AF_INET6,
		    (void *)&a6->sin6_addr, _ip_buffer, sizeof(_ip_buffer));
		if (ret == NULL) {
			perr("Failed to convert addrinfo6 to str: %s",
			    strerror(errno));
			return NULL;
		}
		return ret;
	} else {
		perr("Failed to convert addrinfo to str: Unknown ip family");
		return NULL;
	}
}

static int create_addrinfo(const char *port,
    const char *bind_ips[MAX_BIND_COUNT], size_t bind_ips_len,
    struct addrinfo **out)
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
			perr("Failed to getaddrinfo: %s", gai_strerror(ret));
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
			perr("Failed to getaddrinfo: %s", gai_strerror(ret));
			goto getaddrinfo_err;
		}
	}

	status = 0;
	*out = first;

getaddrinfo_err:
	return status;
}

static inline void pdebug(const char *format, ...)
{
	pthread_mutex_lock(&log_mutex);

	if (!pdebug_enabled) return;
	va_list ap;

	fprintf(stderr, "%s: DEBUG: ", progname);
	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);
	fprintf(stderr, "\n");

	pthread_mutex_unlock(&log_mutex);
}

static inline void pwarn(const char *format, ...)
{
	pthread_mutex_lock(&log_mutex);

	if (!pwarn_enabled) return;
	va_list ap;

	fprintf(stderr, "%s: WARN: ", progname);
	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);
	fprintf(stderr, "\n");

	pthread_mutex_unlock(&log_mutex);
}


static inline void perr(const char *format, ...)
{
	pthread_mutex_lock(&log_mutex);

	if (!perr_enabled) return;
	va_list ap;

	fprintf(stderr, "%s: ERROR: ", progname);
	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);
	fprintf(stderr, "\n");

	pthread_mutex_unlock(&log_mutex);
}
