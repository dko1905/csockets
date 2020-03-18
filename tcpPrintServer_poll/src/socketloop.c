#include "socketloop.h"
#include "log.h"

#include <unistd.h> // For closing fd
#include <strings.h> // For bzero()
#include <stdlib.h> // For exit()
#include <sys/socket.h> // For socket functions
#include <netinet/in.h> // For protocol

#include <poll.h>



void socketloop(uint16_t port, int timeout, int verbose){
	int sockfd = create_socket(port, verbose);
		
	struct pollfd fds[POLLMAX];

	fds[0].fd = sockfd;
	fds[0].events = POLLIN;

	for(int n = 1; n < POLLMAX; n++){
		fds[n].fd = -1; // poll() will ignore -1
		fds[n].events = 0; // No events
	}

	while(1){
		int pollr = poll(fds, POLLMAX, timeout);
		if(pollr < 0){
			perror("poll() failed");
			exit(2);
		}
		else if(pollr == 0){//* Check for timeout
			printf("Timeout, kicking all connected clients\n");
			for(int n = 1; n < POLLMAX; n++){
				struct pollfd *cfd = &fds[n];// Get current fd
				if(cfd->fd != -1 && cfd->revents != POLLERR){
					close(cfd->fd);
				}
			}
		}
		else{
			

			for(int n = 1; n < POLLMAX; n++){
				if((fds[n].revents & POLLIN) > 0 ){
					// Client handle should be here
					clienthandle(&(fds[n]), verbose);
				}
				if((fds[n].revents & POLLERR) > 0){// check for errors
					close(fds[n].fd);// Don't care if it fails to close
					fds[n].fd = -1;
					fds[n].events = 0;
				}
			}

			//****************** check for errors ******************//
			if(fds[0].revents == POLLERR){//* check listener for errors
				perror("socket POLLERR");
				exit(2);
			}
			//****************** check for errors ******************//

			//****************** find space for new client ******************//
			if(fds[0].revents == POLLIN){// client wants to be accepted
				int freespot = -1;
				for(int n = 1; n < POLLMAX; n++){// Find free place
					if(fds[n].fd == -1){
						freespot = n;
						break;
					}
				}
				if(freespot != -1){
					int acceptr = accept(fds[0].fd, NULL, NULL);
					if(acceptr < 0){
						perror("accept() failed");
						//* Don't exit
					}
					else{
						fds[freespot].fd = acceptr;
						fds[freespot].events = POLLIN;
					}
				}
				else{
					int acceptr = accept(fds[0].fd, NULL, NULL);
					if(acceptr < 0){
						perror("accept() failed");
						//* Don't exit
					}
					else{
						char fullmsg[] = "Server is full\n";
						write(acceptr, fullmsg, sizeof(fullmsg));
						close(acceptr);
					}
				}
			}
			//****************** find space for new client ******************//
		}
	}


	if(close(sockfd) < 0){
		perror("close() failed");
		exit(1);
	}
	else{
		LOG("close() finished\n");
	}
}

int create_socket(uint16_t port, int verbose){
	int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(sockfd < 0){
		perror("socket() failed");
		exit(1);
	}
	else if(verbose){
		LOG("socket() finished\n");
	}
	struct sockaddr_in addr;

	bzero((char *) &addr, sizeof(addr));

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(port);

	if(bind(sockfd, (struct sockaddr *) &addr, sizeof(addr)) < 0){
		perror("bind() failed");
		exit(1);
	}
	else if(verbose){
		LOG("bind() finished\n");
	}

	if(listen(sockfd, MAXBACKLOG) < 0){
		perror("listen() failed");
		exit(1);
	}
	else if(verbose){
		LOG("listen() finished\n");
	}
	return sockfd;
}