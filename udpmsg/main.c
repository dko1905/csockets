#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_PACKET_SIZE 1212
#define MAX_ERROR_COUNT 3
#define PORT 10020

int parse_ip(const char *ip, struct sockaddr *addr);
int server(int family, struct sockaddr *listen_addr, socklen_t listen_addr_len);
int client(struct sockaddr *remote_addr);

int main(int argc, char *argv[]) {
	int ret = 0;

	if (argc < 3) {
		fprintf(stderr, "Not enough arguments!\n");
		return EXIT_FAILURE;
	}

	if (strcmp("server", argv[1]) == 0) {
		struct sockaddr addr = {0};
		socklen_t addr_len = 0;
		int family = 0;

		ret = parse_ip(argv[2], &addr);
		if (ret == 0) {
			struct sockaddr_in *addr4 = (struct sockaddr_in *)&addr;

			addr4->sin_port = htons(PORT);
			addr4->sin_family = AF_INET;

			family = AF_INET;
			addr_len = sizeof(*addr4);
		} else if (ret == 1) {
			struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)&addr;

			addr6->sin6_port = htons(PORT);
			addr6->sin6_family = AF_INET6;

			family = AF_INET6;
			addr_len = sizeof(*addr6);
		} else {
			perror("Failed to parse ip");
			return EXIT_FAILURE;
		}

		return server(family, &addr, addr_len);
	} else if (strcmp("client", argv[1]) == 0) {
		fprintf(stderr, "Client yet implemented!\n");
		return EXIT_FAILURE;
	} else {
		fprintf(stderr, "Unknown mode (first argument)!\n");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

/* Returns:
 * -1 - invalid address
 *  0 - ipv4 address
 *  1 - ipv6 address
 */
int parse_ip(const char *ip, struct sockaddr *addr) {
	int ret = 0;
	struct in_addr ipv4 = {0};
	struct in6_addr ipv6 = {0};

	ret = inet_pton(AF_INET, ip, &ipv4);
	if (ret == 1) {
		((struct sockaddr_in *)addr)->sin_addr = ipv4;
		return 0;
	} else if (ret == 0) {
		/* Try next family */
	} else {
		return -1;
	}

	ret = inet_pton(AF_INET6, ip, &ipv6);
	if (ret == 1) {
		((struct sockaddr_in6 *)addr)->sin6_addr = ipv6;
		return 1;
	} else {
		return -1;
	}
}

int server(int family, struct sockaddr *listen_addr, socklen_t listen_addr_len) {
	// server fd, general return value, error count (used to stop loops)
	int servfd = 0, ret = 0, errcount = 0, status = EXIT_FAILURE;
	ssize_t packet_len = 0;

	char buffer[MAX_PACKET_SIZE] = {0};
	size_t buffer_size = MAX_PACKET_SIZE;

	struct sockaddr clientaddr = {0};
	socklen_t clientaddr_len = sizeof(clientaddr);
	#define ipaddr_len 28
	char ipaddr[ipaddr_len] = {0};

	/* Create socket fd. */
	servfd = socket(family, SOCK_DGRAM, 0);
	if (servfd < 0) {
		perror("Failed to create socket");
		goto socket_err;
	}

	/* Bind address. */
	ret = bind(servfd, listen_addr, listen_addr_len);
	if (ret < 0) {
		perror("Failed to bind");
		goto bind_err;
	}

	/* Accept packets. */
	while (1) {
		clientaddr_len = sizeof(clientaddr);
		packet_len = recvfrom(servfd, buffer, buffer_size - 1, 0,
				              &clientaddr, &clientaddr_len);

		/* Check error and handle appropriately. */
		if (packet_len < 0) {
			if (errno == ECONNREFUSED) {
				++errcount;
				fprintf(stderr,
				        "Failed to receive: Connection refused (%d try): %s",
				        errcount, strerror(errno));
				if (errcount > 3) {
					goto recv_err;
					break;
				}
			} else {
				perror("Failed to receive");
				goto recv_err;
			}
		} else if (packet_len == 0) {
			/* 0 len packet catch. */
			printf("INFO: Received 0 byte dgram.\n");
		} else {
			/* Normal packet catch. */
			if (clientaddr_len >= 28) {
				struct sockaddr_in6* addr = (struct sockaddr_in6*)&clientaddr;
				const char *ret = inet_ntop(AF_INET6,
				                            &addr->sin6_addr, ipaddr,
				                            ipaddr_len);
				if (ret == NULL) perror("Failed to convert ip");
				printf("ipv6\n");
			} else {
				struct sockaddr_in* addr = (struct sockaddr_in*)&clientaddr;
				const char *ret = inet_ntop(AF_INET, &addr->sin_addr, ipaddr,
				                            ipaddr_len);
				if (ret == NULL) perror("Failed to convert ip");
				printf("ipv4\n");
			}


			buffer[packet_len] = '\0';
			for (size_t n = packet_len - 1; buffer[n] == '\n'; --n) {
				buffer[n] = '\0';
			}
			printf("Received (%s %zd): \"%s\"\n",
			       ipaddr, packet_len, buffer);
		}
	}

	status = EXIT_SUCCESS;

	recv_err:
	bind_err:
	if (close(servfd) != 0) perror("Failed to close servfd");
	socket_err:
	return status;
}


