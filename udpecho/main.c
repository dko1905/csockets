#include <sys/types.h>
#include <sys/socket.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netdb.h>

#define MAX(_a, _b) ((_a > _b) ? _a : _b)

#define MAX_THERAD_COUNT 10

static int print_addr(const char *progname, const struct addrinfo *addr) {
	char ipstr_all[MAX(INET_ADDRSTRLEN, INET6_ADDRSTRLEN)] = {0};
	struct in_addr  addr4 = {0};
	struct in6_addr addr6 = {0};
	void *addr_all = NULL;

	if (addr->ai_family == AF_INET) {
		addr4 = ((struct sockaddr_in *)addr->ai_addr)->sin_addr;
		addr_all = &addr4;
	} else if (addr->ai_family == AF_INET6) {
		addr6 = ((struct sockaddr_in6 *)addr->ai_addr)->sin6_addr;
		addr_all = &addr6;
	} else {
		fprintf(stderr, "%s: ipv4 and ipv6 not detected;"
		        " unsupported protocol\n", progname);
		return EXIT_FAILURE;
	}

	const char *ret2 = inet_ntop(addr->ai_family,
	                             addr_all, ipstr_all,
	                             addr->ai_addrlen);
	if (ret2 != ipstr_all) {
		fprintf(stderr, "%s: %s\n", progname, strerror(errno));
		return EXIT_FAILURE;
	}

	fprintf(stderr, "%s: Configured listening address on %s\n", progname,
	        ipstr_all);

	return EXIT_SUCCESS;
}

static void *loop_func(void *args);

int main(int argc, char *argv[]) {
	char *port = NULL;
	struct addrinfo hints = {0}, *addr = NULL, *na = NULL;
	pthread_t thread_arr[MAX_THERAD_COUNT] = {0};
	/* Array telling if thread is empty. 1-valid,0-invalid */
	int threadvalid_arr[MAX_THERAD_COUNT] = {0};
	int sfd_arr[MAX_THERAD_COUNT] = {0};
	int ret = 0, sfd_len = 0;

	/* Read & parse arguments */
	if (argc < 2) {
		fprintf(stderr, "%s: Not enough arguments\n", argv[0]);
		return EXIT_FAILURE;
	}
	port = argv[1];

	/* Setup hints */
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;
	hints.ai_protocol = IPPROTO_UDP;

	/* Get listening address */
	ret = getaddrinfo(NULL, port, &hints, &addr);
	if (ret != 0) {
		fprintf(stderr, "%s: %s\n", argv[0], gai_strerror(ret));
		return EXIT_FAILURE;
	} else {
		/* Print addresses */
		na = addr;
		while (na != NULL) {
			ret = print_addr(argv[0], na);
			if (ret != EXIT_SUCCESS) return EXIT_FAILURE;

			na = na->ai_next;
		}
	}

	/* Bind socket and then start master loop in new thread. */
	size_t n = 0;
	for (na = addr; na != NULL; na = na->ai_next, n++) {
		/* Create socket. */
		int sfd = socket(na->ai_family, na->ai_socktype, na->ai_protocol);
		if (sfd == -1) {
			fprintf(stderr, "%s: Failed create socket of: %s\n", argv[0],
			        strerror(errno));
			print_addr(argv[0], na);
			continue;
		}

		/* Bind socket to address. */
		ret = bind(sfd, na->ai_addr, na->ai_addrlen);
		if (ret != 0) {
			fprintf(stderr, "%s: Failed to bind fd of: %s\n", argv[0],
			        strerror(errno));
			print_addr(argv[0], na);
			close(sfd);
			continue;
		}

		/* Create new thread that handles requests. */
		ret = pthread_create(&(thread_arr[n]), NULL, loop_func, &(sfd_arr[n]));
		if (ret != 0) {
			fprintf(stderr, "%s: Failed to create thread for: %s\n", argv[0],
			        strerror(errno));
			print_addr(argv[0], na);
			close(sfd);
			thread_arr[n] = 0;
			threadvalid_arr[n] = 0;
			continue;
		}
		threadvalid_arr[n] = 1;

		++sfd_len;
	}
	fprintf(stderr, "%s: Binded on %d address(es)\n", argv[0], sfd_len);

	freeaddrinfo(addr);

	for (size_t n = 0; n < MAX_THERAD_COUNT; ++n) {
		if (threadvalid_arr[n] == 1) {
			ret = pthread_join(thread_arr[n], NULL);
			if (ret != 0) {
				fprintf(stderr, "%s: %s\n", argv[0], strerror(errno));
			}
		}
	}

	return 0;
}

static void *loop_func(void *args) {
	printf("thread sfd %d\n", *((int *)args));

	return NULL;
}
