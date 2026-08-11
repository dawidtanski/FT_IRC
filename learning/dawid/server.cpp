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
#include <vector>

#define PORT "4242"
#define BACKLOG 10


// Function to handle partial sending. Send might not send all the bytes we want to, so it's 
// necessary to avoid this type of situation. To do this we use sendall

//		ssize_t send(int sockfd, const void buf[.len], size_t len, int flags);
int sendall(int sock_fd, char *buf, int *len){
	
	int total = 0;
	int bytesLeft = *len;
	int n = 0;

	while (total < *len){
		n = send(sock_fd, buf+total, bytesLeft, 0);
		if (n == -1)
			break;
		total += n;
		bytesLeft -= n;
	}
	*len = total;

	return n == -1 ? -1 : 0;

}


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
	if ((status = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0)
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
	
	if (listen(listener, BACKLOG) == -1)
		throw std::runtime_error("listening error");
	return listener;
}

// Dealing with FDs set

void addToPfds(std::vector<struct pollfd>& pfds, int newFD)
{
	struct pollfd pfd;

	pfd.fd = newFD;
	pfd.events = POLLIN;
	pfd.revents = 0;

	pfds.push_back(pfd);
}


// void addToPfds(struct pollfd **pfds, int newfd, int *fd_coun)


void handleNewConnection(int listener, std::vector<struct pollfd>& pfds){

	socklen_t addrLen;
	int newFD;
	struct sockaddr_storage clientAddr;
	char clientIP[INET6_ADDRSTRLEN];

	addrLen = sizeof(clientAddr);
	newFD = accept(listener, (struct sockaddr *)&clientAddr, &addrLen);
	if (newFD == -1)
		throw std::runtime_error("Failed to accept the conneciton");
	else{
		addToPfds(pfds, newFD);
		std::cout << "pollserver:newconnectionfrom " << inet_ntop2(clientAddr) <<
		newFD << std::endl;
	}
}

// for testing purposes broadcast a message to all clients

void broadcast(char *buf, int nbytes, int listener, int s, std::vector<struct pollfd>& pfds){

	for(int i = 0; i < pfds.size(); i++){
		// checking if fd is included in master set
		if (pfds[i].fd != listener && pfds[i].fd != s){

			int bytesToSend = nbytes;
			if (sendall(pfds[i].fd, buf, &bytesToSend) == -1)
				throw std::runtime_error("Failed to send the data");
		}
			}
}

// Function that handles client data
void handleUpcomingData(int s, int listener, std::vector<struct pollfd>& pfds, int index){

	char buf[256];
	int nbytes;

	if ((nbytes = recv(s, buf, sizeof buf, 0)) <= 0){
		if (nbytes == 0)
			std::cout << "Socket " << s << " hung up." << std::endl;
		else
			throw std::runtime_error("Failed to receive data from client");
		close(s);
		pfds.erase(pfds.begin() + index); 
	}else{
		broadcast(buf, nbytes, listener, s, pfds);
	}
}

void handlePollEvents(int listener, std::vector<struct pollfd>& pfds){
	
	for(int i = 0; i < pfds.size(); i++){
		if (pfds[i].revents & (POLLIN | POLLHUP)){
			if (pfds[i].fd == listener)
				handleNewConnection(listener, pfds);
			else
				handleUpcomingData(pfds[i].fd, listener, pfds, i);
		}
	}
}


int main(void)
{
    int listener;        // Listening socket descriptor

	std::vector<struct pollfd> pfds;

    // Setup and get a listening socket
	listener = prepareSocket();
	if (listener == -1) {
		throw std::runtime_error("Couldn't prepare socket");
		exit(1);
	}

    // Add the listener to the pollfd set
	addToPfds(pfds, listener);
	pfds[0].fd     = listener;
	pfds[0].events = POLLIN;

	puts("pollserver: waiting for connections...");

	// Main loop
	for (;;) {
		int poll_count = poll(pfds.data(), pfds.size(), -1);

		if (poll_count == -1) {
			perror("poll");
			exit(1);
		}

		// Run through connections looking for data to read
		handlePollEvents(listener, pfds);
	}

	return 0;
}
