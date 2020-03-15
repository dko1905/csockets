#include <stdio.h>
#include <strings.h> // For bzero()
#include <stdlib.h> // For exit()

#include <sys/socket.h> // For socket functions
#include <netinet/in.h> // For protocol

#include <unistd.h> // For closing fd

#define LOG(...) printf(__VA_ARGS__)

#define MAXBACKLOG 10

void socket_v4(int verbose);
void client_handle(int clientfd, int verbose);

int main(){
	socket_v4(1);
	return 0;
}



void socket_v4(int verbose){
	int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(sockfd < 0){
		perror("Erorr in socket()");
		exit(1);
	}
	else if(verbose){
		LOG("socket() finished\n");
	}
	struct sockaddr_in addr;
	
	bzero((char *) &addr, sizeof(addr));
	int portno = 8999;

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(portno);

	if(bind(sockfd, (struct sockaddr *) &addr, sizeof(addr)) < 0){
		perror("Failed to bind()");
		exit(1);
	}
	else if(verbose){
		LOG("bind() finished\n");
	}

	if(listen(sockfd, MAXBACKLOG) < 0){
		perror("Failed to listen()");
		exit(1);
	}
	else if(verbose){
		LOG("listen() finished\n");
	}

	while(1){
		int clientfd = accept(sockfd, NULL, NULL);
		if(clientfd < 0){
			perror("accept() failed");
			//* Do not exit
		}
		else if(verbose){
			LOG("accept() finished\n");
		}

		client_handle(clientfd, verbose);

		if(close(clientfd) < 0){
			perror("Failed to close connection to client");
		}
		else if(verbose){
			LOG("Failed to close connection to client\n");
		}
	}

	if(close(sockfd) < 0){
		perror("close() failed");
		exit(1);
	}
	else if(verbose){
		LOG("close() finished\n");
	}
}

#define LEN 255

void client_handle(int clientfd, int verbose){
	

	char buffer[LEN];
	int bytesRemaining = LEN;
	while(bytesRemaining > 0){
		int bread = read(clientfd, &buffer, bytesRemaining);
		if(bread < -1){
			perror("Failed to read");
			return;
		}
		bytesRemaining = bytesRemaining-bread;
	}

	if(write(clientfd, buffer, LEN) < 0){
		perror("Failed to write");
		return;
	}
}