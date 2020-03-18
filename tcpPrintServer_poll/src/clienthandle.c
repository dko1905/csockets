#include "socketloop.h"
#include "log.h"

#include <sys/socket.h> // For socket functions
#include <poll.h> // For POLLERR
#include <unistd.h> // for read
#include <stdio.h> // for printf

// fd is just a pointer to the fdstruct //* IT'S NOT AN ARRAY
void clienthandle(struct pollfd *fds, int verbose){ 
	char buffer[11];
	int bytesRead=0; 
	if((bytesRead = read(fds->fd, buffer, 10)) <= 0){
		if(verbose)
			LOG("Failed to read() in clienthandle\n");
		fds->revents = POLLERR; //* Needs to be over 'check for errors'
	}
	else{
		buffer[bytesRead] = '\0';
		if(write(fds->fd, buffer, bytesRead) <= 0){
			if(verbose)
				LOG("Failed to write() in clienthandle");			
		}
		if(verbose)
			LOG("Wrote packet back\n");
	}
}