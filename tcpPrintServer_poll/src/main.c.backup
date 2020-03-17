#include <stdio.h>
#include <unistd.h>
#include <poll.h>

#include <stdlib.h>

#include <fcntl.h> // Open()

int main(){
	int fd = 0; // STDIN

	struct pollfd fds[1];
	int timeout = 5000, fdsLen; // 20s

	char buffer[11];

	while(1){
		fds[0].fd = fd;
		fds[0].events = 0 | POLLIN; 
		fdsLen = 1;

		int pollReturn = poll(fds, fdsLen, timeout);
		
		if(pollReturn < 0){
			perror("poll() failed");
			exit(1);
		}
		else if(pollReturn == 0){
			printf("Timeout\n");
		}
		else{
			for(int n = 0; n < fdsLen; n++){
				if(fds[n].revents == POLLIN){
					int readResult = read(fds[n].fd, buffer, 10);
					if(readResult < 0){
						perror("Read failed!");
						exit(1);
					}
					else if(readResult == 0){ // EOF
						exit(0);
					}
					else{
						buffer[readResult] = '\0';
						printf("%s", buffer);
					}
				}
			}
		}
	}
}