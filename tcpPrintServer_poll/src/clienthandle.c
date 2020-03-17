#include "socketloop.h"

#include <sys/socket.h> // For socket functions
#include <poll.h> // For POLLERR
#include <unistd.h> // for read
#include <stdio.h> // for printf

// fd is just a pointer to the fdstruct //* IT'S NOT AN ARRAY
void clienthandle(struct pollfd *fds, int verbose){ 
	char buffer[11];
	int bytesRead=0; 
	if((bytesRead = read(fds->fd, buffer, 10)) <= 0){
		fds->revents = POLLERR; //* Needs to be over 'check for errors'
	}
	if(bytesRead > 0)	{
		buffer[bytesRead] = '\0';
		printf("%s\n", buffer);
	}
}