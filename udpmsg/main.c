#include <asm-generic/errno.h>
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

int server(struct sockaddr *listen_addr, socklen_t listen_addr_len);
int client(struct sockaddr *remote_addr);

int main(int argc, char *argv[]) {
	if (argc < 2) {
		fprintf(stderr, "Not enough arguments!\n");
		return EXIT_FAILURE;
	}

	if (strcmp("server", argv[1]) == 0) {
		struct sockaddr_in6 address = {0};
		uint8_t loopback[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1};

		address.sin6_family = AF_INET6;
		address.sin6_port = htons(PORT);
		memcpy(address.sin6_addr.s6_addr, loopback, sizeof(loopback));

		return server((struct sockaddr*)&address, sizeof(address));
	} else if (strcmp("client", argv[1]) == 0) {
		fprintf(stderr, "Client yet implemented!\n");
		return EXIT_FAILURE;
	} else {
		fprintf(stderr, "Unknown mode (first argument)!\n");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

int server(struct sockaddr *listen_addr, socklen_t listen_addr_len) {
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
	servfd = socket(AF_INET6, SOCK_DGRAM, 0);
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
		packet_len = recvfrom(servfd, buffer, buffer_size - 1, 0,
				              &clientaddr, &clientaddr_len);

		/* Check error and handle appropriately. */
		if (packet_len < 0) {
			if (errno == ECONNREFUSED) {
				++errcount;
				if (errcount > 3) {
					perror("Failed to receive: Connection refused");
					goto recv_err;
					break;
				}
			} else {
				perror("Failed to receive");
				goto recv_err;
			}
		} else if (packet_len == 0) {
			/* 0 len packet catch. */
			printf("INFO: ha ha, received 0 byte dgram.\n");
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
