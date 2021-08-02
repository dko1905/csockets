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
#include <signal.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

#include "util/net.h"
#include "users.h"

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
/* = net = */
#ifndef MAX_BIND_COUNT
/* Max count of interfaces to listen on. Normally only two are used. */
#define MAX_BIND_COUNT (10)
#endif
#ifndef MAX_MSG_SIZE
/* Max size of message that can be received. */
#define MAX_MSG_SIZE (2048)
#endif
#ifndef ACTUSER_TIMEOUT
/* Timeout before removeing user from active users (in seconds). */
#define ACTUSER_TIMEOUT (3000)
#endif

/* == Globals == */
static char *progname = "";

/* == Static functions == */
static void *master_func(void *args);
struct master_func_args {
	int *sfd_arr;
	size_t sfd_arr_len;
	int send_fd4, send_fd6;
	volatile bool *run;
};
/* On message handle.
 *
 * Returns:
 * 0 - success
 * 1 - error
*/
static int msg_handle(int sfd, struct master_func_args *mf_args,
    struct user_table *active_users);
/* sendall_func is called by user_table_every to send message to other users. */
static void sendall_func(const struct user *user, void *args0);
struct sendall_func_args {
	int sfd;
	int send_fd4, send_fd6;
	char *buffer;
	size_t buffer_len;
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
	int send_fd4 = 0, send_fd6 = 0;
	/* Args to master_func. */
	struct master_func_args master_args = {0};
	bool master_args_run = true;
	/* Catch sigTERM and shutdown gracefully. */
	sigset_t catchset = {0};

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

	/* Setup catchset. */
	ret = sigemptyset(&catchset);
	if (ret != 0) {
		perror("Failed to setup catchset: %s", strerror(errno));
		goto catchset_err;
	}
	ret = sigaddset(&catchset, SIGTERM);
	ret += sigaddset(&catchset, SIGINT);
	if (ret != 0) {
		perror("Failed to add SIGTERM or SIGINT to catchset: %s",
		    strerror(errno));
		goto catchset_err;
	}
	ret = sigprocmask(SIG_BLOCK, &catchset, NULL);
	if (ret != 0) {
		perror("Failed to set block policy on SIGTERM or SIGINT: %s",
		    strerror(errno));
		goto catchset_err;
	}

	/* Create send_fd4 & 6. */
	ret = socket(AF_INET, SOCK_DGRAM, 0);
	if (ret < 0) {
		perror("Failed to setup send_fd4: %s", strerror(errno));
		goto send_fd4_err;
	}
	send_fd4 = ret;
	ret = socket(AF_INET6, SOCK_DGRAM, 0);
	if (ret < 0) {
		perror("Failed to setup send_fd6: %s", strerror(errno));
		goto send_fd6_err;
	}
	send_fd6 = ret;

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
	master_args.send_fd4 = send_fd4;
	master_args.send_fd6 = send_fd6;
	master_args.run = &master_args_run; /* I think this makes it volatile. */
	ret = pthread_create(&master_thread, NULL, master_func, &master_args);
	if (ret != 0) {
		perror("Failed to create master thread: %s", strerror(ret));
		goto pthread_err;
	}

	/* Wait until main thread catches SIGINT or SIGTERM, and shutdown.
	 * Using ret two times might be UB.
	 */
	ret = sigwait(&catchset, &ret);
	if (ret != 0) {
		pwarn("sigwait: %s", strerror(errno));
	} else {
		pdebug("sigwait: closing program nicely");
	}

	/* Set run to false, and shutdown all sockets. Shutting down the
	 * sockets will send an event to poll which will shut down the thread.
	 */
	*master_args.run = false;
	for (size_t n = 0; n < sfd_arr_len; ++n) {
		shutdown(sfd_arr[n], SHUT_RDWR);
	}
	pthread_join(master_thread, NULL);
pthread_err:
	for (size_t n = 0; n < sfd_arr_len; ++n) {
		close(sfd_arr[n]);
	}
socket_err:
	freeaddrinfo(addr);
addrinfo_err:
	close(send_fd6);
send_fd6_err:
	close(send_fd4);
send_fd4_err:
catchset_err:
args_err:
	return status;
}

static void *master_func(void *args0)
{
	struct master_func_args *args = (struct master_func_args *)args0;
	/* Poll array to poll(3). */
	struct pollfd poll_arr[MAX_BIND_COUNT] = {0};
	size_t poll_arr_len = 0;
	/* General return from various functions. */
	int ret = 0;
	/* Active user ring buffer. */
	struct user_table active_users = {0};

	/* Setup active users queue. */
	ret = user_table_init(&active_users, ACTUSER_TIMEOUT);
	if (ret != 0) {
		perror("Failed to create active users: %s", strerror(errno));
		goto user_queue_err;
	}

	/* Fill poll array. */
	for (size_t n = 0; n < args->sfd_arr_len && n < MAX_BIND_COUNT; ++n) {
		poll_arr[n].fd = args->sfd_arr[n];
		/* POLLERR & POLLHUP are not required, but added for compat. */
		poll_arr[n].events = POLLIN | POLLPRI | POLLERR | POLLHUP;
		/* Rest of element is 0. */
		++poll_arr_len;
	}
	for (size_t n = poll_arr_len; n < MAX_BIND_COUNT; ++n) {
		poll_arr[n].fd = -1; /* poll **will** ignore under 0. */
		/* Rest of element is 0. */
	}

	while (*args->run == true) {
		/* Block until event arrives. */
		ret = poll(poll_arr, poll_arr_len, -1);
		if (ret == -1) {
			if (errno == EINTR) {
				pwarn("poll: Caught interrupt, continueing");
				continue;
			} else {
				perror("poll: %s", strerror(errno));
				goto poll_err;
			}
		} else if (ret == 0) {
			pwarn("poll: Returned 0 even though timeout is inf");
			continue;
		}

		/* Check for file descriptors with events. */
		for (size_t n = 0; n < poll_arr_len; ++n) {
			int sfd = poll_arr[n].fd;
			/* Check for POLLIN,POLLPRI,POLLERR and POLLHUP. */
			switch (poll_arr[n].revents) {
			case POLLIN: /* FALLTHROUGH*/
			case POLLPRI:
				ret = msg_handle(sfd, args, &active_users);
				if (ret != 0) {
					goto poll_err;
				}
				break;
			case POLLERR:
				perror("%d: POLLERR", sfd);
				goto poll_err;
			case POLLHUP:
				if (*args->run != false) {
					perror("%d: POLLHUP", sfd);
				}
				goto poll_err;
			}
		}
	}

poll_err:
	user_table_free(&active_users);
user_queue_err:
	return NULL;
}

static int msg_handle(int sfd, struct master_func_args *mf_args,
    struct user_table *active_users)
{
	int ret = 0;
	struct sendall_func_args sendall_args = {0};
	/* Receive buffer. */
	char buffer[MAX_MSG_SIZE] = {0};
	size_t buffer_len = 0;
	struct sockaddr_storage peeraddr = {0};
	socklen_t peeraddr_len = sizeof(peeraddr);
	/* Created user from ip. */
	struct user user = {0};

	/* Read UDP packet. */
	ret = recvfrom(sfd, buffer, sizeof(buffer), 0,
	    (struct sockaddr *)&peeraddr, &peeraddr_len);
	/* When ret is 0 either the datagram is 0
	 * in size, or socket is closed. We will treat
	 * it as a "0-datagram".
	 */
	if (ret == 0) {
		/* Ignore zero len datagram. */
#if PRINT_DEBUG == 1
		pdebug("%d: recvfrom: 0-datagram", sfd);
#endif
		return 0;
	} else if (ret == -1) {
		/* Ignore theese errors. */
		switch (errno) {
		case EIO:
		case ECONNRESET:
		case EINTR:
		case ETIMEDOUT:
#if PRINT_DEBUG == 1
			pdebug("%d: ignore error: %d",
			    sfd, errno);
#endif
			return 0;
		default:
			perror("%d: recvfrom: %s", sfd, strerror(errno));
			return 1;
		}
	} else {
		buffer_len = ret;
	}
	/* Create user and add to table (will allocate on heap). */
#if PRINT_DEBUG == 1
	pdebug("recv from '%s'", addr2str(AF_INET, (void *)&peeraddr));
#endif
	user.addr = peeraddr;
	user.addr_len = peeraddr_len;
	user.messages_in5s = 0;

	ret = user_table_update(active_users, &user);
	if (ret != 0) {
		perror("Failed to update active user table: %s",
		    strerror(errno));
		return 1;
	}

	sendall_args.buffer = buffer;
	sendall_args.buffer_len = buffer_len;
	sendall_args.sfd = sfd;
	sendall_args.send_fd4 = mf_args->send_fd4;
	sendall_args.send_fd6 = mf_args->send_fd6;
	user_table_every(active_users, sendall_func, &sendall_args);

	return 0;
}

static void sendall_func(const struct user *user, void *args0) {
	struct sendall_func_args *args = args0;
	ssize_t bytes = 0;

	if (user->addr_len == sizeof(struct sockaddr_in)) {
		bytes = sendto(args->send_fd4, args->buffer, args->buffer_len,
		    0, (void *)&user->addr, user->addr_len);
		if (bytes < 1) {
			perror("%d: sendto (%s): %s", args->sfd,
			    addr2str(AF_INET, (void *)&user->addr),
			    strerror(errno));
		} else {
#if PRINT_DEBUG == 1
			pdebug("%d: sendto (%s): %zd bytes", args->sfd,
			    addr2str(AF_INET, (void *)&user->addr),
			    bytes);
#endif
		}
	} else {
		bytes = sendto(args->send_fd6, args->buffer, args->buffer_len,
		    0, (void *)&user->addr, user->addr_len);
		if (bytes < 1) {
			perror("%d: sendto (%s): %s", args->sfd,
			    addr2str(AF_INET6, (void *)&user->addr),
			    strerror(errno));
		} else {
#if PRINT_DEBUG == 1
			pdebug("%d: sendto (%s): %zd bytes", args->sfd,
			    addr2str(AF_INET6, (void *)&user->addr),
			    bytes);
#endif
		}
	}
}
