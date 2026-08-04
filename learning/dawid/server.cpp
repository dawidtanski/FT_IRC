#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>

#define PORT 


// Function to handle partial sending. Send might not send all the bytes we want to, so it's 
// necessary to avoid this type of situation. To do this we use sendall

//		ssize_t send(int sockfd, const void buf[.len], size_t len, int flags);
int sendall(int sock_fd, char *buf, int *len){
	
	int total = 0;
	int bytesLeft = *len;
	int n;

	while(total < *len){
		n = send(sock_fd, buf+total, bytesLeft, 0);
		if (n == -1)
			break;
		total += n;
		bytesLeft -= n;
	}
	*len = total;

	return n == -1 ? -1 : 0;

}