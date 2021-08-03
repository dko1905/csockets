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

#include "config.h"

#include "util/print.h"
#include "util/net.h"
#include "users.h"
#include "msg_formatter.h"

/* == Globals == */
static char *progname = "";

/* == Static functions == */
static void *master_func(void *args);
struct master_func_args {
	int *sfd_arr;
	size_t sfd_arr_len;
	volatile bool *run;
};
/* On message handle.
 *
 * Returns:
 * 0 - success
 * 1 - error
*/
static int msg_handle(int sfd, struct user_table *active_users);
/* sendall_func is called by user_table_every to send message to other users. */
static void sendall_func(const struct user *user, void *args0);
struct sendall_func_args {
	struct user *sender;
	char *buffer;
	size_t buffer_len;
};
/* timeout_func is called by user_table_update and will be called with timeed-
 * out users.
 */
static void timeout_func(const struct user *user, void *arg0);
struct timeout_func_args {
	/* empty */
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
		pinfo("Bound %s:%s", addr2str(ca->ai_family, ca->ai_addr), port);

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
		pinfo("sigwait: closing program nicely");
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
		poll_arr[n].fd = -1; /* poll **will** ignore values under 0. */
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
			/* Check for POLLIN,POLLPRI,POLLERR and POLLHUP.
			 * The current impl does not support multiple events!
			 */
			switch (poll_arr[n].revents) {
			case POLLIN: /* FALLTHROUGH*/
			case POLLPRI:
				ret = msg_handle(sfd, &active_users);
				if (ret != 0) {
					perror("msg_handle error");
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

static int msg_handle(int sfd, struct user_table *active_users)
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
		pdebug("%d: recvfrom: 0-datagram", sfd);
		return 0;
	} else if (ret == -1) {
		/* Ignore theese errors. */
		switch (errno) {
		case EIO:
		case ECONNRESET:
		case EINTR:
		case ETIMEDOUT:
			pdebug("%d: ignore error: %d",
			    sfd, errno);
			return 0;
		default:
			perror("%d: recvfrom: %s", sfd, strerror(errno));
			return 1;
		}
	} else {
		buffer_len = ret;
	}
	/* Create user and add to table (will allocate on heap). */
	user.addr = peeraddr;
	user.addr_len = peeraddr_len;
	if (peeraddr_len == sizeof(struct sockaddr_in))
		user.addr_family = AF_INET;
	else if (peeraddr_len == sizeof(struct sockaddr_in6))
		user.addr_family = AF_INET6;
	else {
		perror("%d: recvfrom (?): Unknown family (%d socklen)", sfd,
		    peeraddr_len);
		return 1;
	}
	user.id = user_calculate_id(&user);
	user.recv_fd = sfd;
	user.last_msg = time(NULL);
	/* Print debug infomation. */
	pdebug("%d: recvfrom (%s): %zd bytes", sfd,
	    addr2str(user.addr_family, (void *)&user.addr),
	    buffer_len);

	ret = user_table_update(active_users, &user, timeout_func, NULL);
	if (ret != 0) {
		perror("Failed to update active user table: %s",
		    strerror(errno));
		return 1;
	}

	sendall_args.sender = &user;
	sendall_args.buffer = buffer;
	sendall_args.buffer_len = buffer_len;
	user_table_every(active_users, sendall_func, &sendall_args);

	return 0;
}

static void sendall_func(const struct user *user, void *args0)
{
	struct sendall_func_args *args = args0;
	ssize_t bytes = 0;
	size_t buffer_len = args->buffer_len;
	const char *send_buffer = NULL;

	if (user->id == args->sender->id) {
		send_buffer = msg_formatter(args->sender->id, user->id,
		    args->buffer, &buffer_len);
	} else {
		send_buffer = msg_formatter(args->sender->id, user->id, args->buffer,
		    &buffer_len);
	}

	bytes = sendto(user->recv_fd, send_buffer, buffer_len, 0,
	    (void *)&user->addr, user->addr_len);
	if (bytes < 1) {
		perror("%d: sendto (%s): %s", user->recv_fd,
		    addr2str(user->addr_family, (void *)&user->addr),
		    strerror(errno));
		return;
	}
	/* Print debugging infomation. */
	pdebug("%d: sendto (%s): %zd bytes", user->recv_fd,
	    addr2str(user->addr_family, (void *)&user->addr), bytes);
}

static void timeout_func(const struct user *user, void *args0)
{
	struct timeout_func_args *args = args0;
	const char *send_buffer = NULL;
	size_t send_buffer_len = sizeof(MSG_USR_TIMEOUT_STR);
	ssize_t bytes = 0;

	(void)args; /* We don't use it yet. */
	send_buffer = msg_formatter(0, user->id, MSG_USR_TIMEOUT_STR,
	    &send_buffer_len);
	bytes = sendto(user->recv_fd, send_buffer, send_buffer_len, 0,
	    (void *)&user->addr, user->addr_len);
	if (bytes < 1) {
		perror("%d: sendto (%s): %s", user->recv_fd,
		    addr2str(user->addr_family, (void *)&user->addr),
		    strerror(errno));
		return;
	}
	/* Print debugging infomation. */
	pdebug("%d: sendto (%s): %zd bytes", user->recv_fd,
	    addr2str(user->addr_family, (void *)&user->addr), bytes);
}
