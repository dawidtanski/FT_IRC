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
#include <iostream>

#define PORT "4242"


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

/*struct sockaddr_in6 {
    uint16_t       sin6_family;   // AF_INET6
    uint16_t       sin6_port;     // port
    uint32_t       sin6_flowinfo; // informacje o przepływie
    struct in6_addr sin6_addr;    // <-- ADRES IPv6
    uint32_t       sin6_scope_id; // scope
};*/

//inet_ntop(AF_INET, &(sa.sin_addr), ip4, INET_ADDRSTRLEN);
// Function that converts ip from "network to presentation"
std::string inet_ntop2(const sockaddr_storage& addr){

	char buf[INET6_ADDRSTRLEN];

	if (addr.ss_family == AF_INET){
		const sockaddr_in *sa4 = (const sockaddr_in *)&addr;
		if (!inet_ntop(sa4->sin_family, &sa4->sin_addr, buf, INET6_ADDRSTRLEN))
			throw std::runtime_error("inet_ntop failed");
		return buf;
	}
	else if (addr.ss_family == AF_INET6){
		const sockaddr_in6 *sa6 = (const sockaddr_in6 *)&addr;
		if(!inet_ntop(sa6->sin6_family, &sa6->sin6_addr, buf, INET6_ADDRSTRLEN))
			throw std::runtime_error("inet_ntop failed");
		return buf;
	}
	else
		throw std::runtime_error("Couldn't convert address from binary to string");
}

// Consider using two sockets - one for ipv4 and another one for ipv6

// Function that creates and prepaire server's listening socket
int prepareSocket(void){

	struct addrinfo hints, *servinfo, *p;
	int yes = 1;
	int status;
	int listener;
	
	// Filling out hints struct with revelant infos
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	// Creating and prepairing data to connection
	if (status = getaddrinfo(NULL, PORT, &hints, &servinfo) != 0)
		throw std::runtime_error("getaddrinfo terminating failed");

	for (p = servinfo; p!= NULL; p = p->ai_next){
		listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (listener < 0)
			continue;

		// Let the new socket use same port/address after app restart
		setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

		if (bind(listener, p->ai_addr, p->ai_addrlen) < 0){
			close(listener);
			continue;
		}
		break;
	}

	// If we didn't get bound
	if (p == NULL)
		throw std::runtime_error("selectserver: failed to bind");
	freeaddrinfo(servinfo);
}