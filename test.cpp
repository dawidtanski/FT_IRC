/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   test.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: kjamrosz <kjamrosz@student.42warsaw.pl>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/08/07 11:50:25 by kjamrosz          #+#    #+#             */
/*   Updated: 2026/08/09 13:08:24 by kjamrosz         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

// #include <sys/types.h>
// #include <sys/socket.h>
// #include <netdb.h>

// int status;
// struct addrinfo hints;
// struct addrinfo *servinfo;  // will point to the results

// memset(&hints, 0, sizeof hints); // make sure the struct is empty
// hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
// hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
// hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

// if ((status = getaddrinfo(NULL, "3490", &hints, &servinfo)) != 0) {
//     fprintf(stderr, "gai error: %s\n", gai_strerror(status));
//     exit(1);
// }

// // servinfo now points to a linked list of 1 or more
// // struct addrinfos

// // ... do everything until you don't need servinfo anymore ....

// freeaddrinfo(servinfo); // free the linked-list

// =======================================================

#include <iostream>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <unistd.h>

#include <arpa/inet.h>
#include <string.h>
#include <string>


int main()
{
	// 1. create a socket
	int listening = socket(AF_INET, SOCK_STREAM, 0);
			// AF_INET - IPv4
	if (listening == -1)
	{
		std::cerr << "Can't create a socket!";
		return (-1);
	}


	// 2. bind the socket to a IP / port
	sockaddr_in hint;
	hint.sin_family = AF_INET;
	hint.sin_port = htons(54001); // big/little-endian

	inet_pton(AF_INET, "0.0.0.0", &hint.sin_addr);

	if (bind(listening, (sockaddr *)&hint, sizeof(hint)) == -1)
	{
		std::cerr << "Can't bind to IP/port";
		return (-2);
	}


	// 3. mark the socket for listening in
	if (listen(listening, SOMAXCONN) == -1)
	{
		std::cerr << "Can't listen!";
		return (-3);
	}


	// 4. accept a call
	sockaddr_in	client;
	socklen_t	clientSize = sizeof(client);
	char		host[NI_MAXHOST];
	char		svc[NI_MAXSERV];

	int clientSocket = accept(listening, (sockaddr *)&client, &clientSize);

	if (clientSocket == -1)
	{
		std::cerr << "Problem with client connecting!";
		return (-4);
	}

	// 5. close the listening socket
	close(listening);

	memset(host, 0, NI_MAXHOST);
	memset(svc, 0, NI_MAXSERV);


	int result = getnameinfo((sockaddr *)&client, sizeof(client), host,
					NI_MAXHOST, svc, NI_MAXSERV, 0);

	if (result)
	{
		std::cout << host << " connected on " << svc << std::endl;
	}
	else
	{
		inet_ntop(AF_INET, &client.sin_addr, host, NI_MAXHOST);
		std::cout << host << " connected on " << ntohs(client.sin_port) << std::endl;
	}

	// 6. while receiving, display message, echo message

	char buf[4096];
	while (true)
	{
		// clear the buffer
		memset(buf, 0, 4096);
		// wait for a message
		int bytesRecv = recv(clientSocket, buf, 4096, 0);
		// display message
		if (bytesRecv == -1)
		{
			std::cerr << "There was a connection issue" << std::endl;
			break;
		}
		if (bytesRecv == 0)
		{
			std::cerr << "The client disconnected" << std::endl;
			break;
		}

		// display message
		std::cout << "Received: " << std::string(buf, 0, bytesRecv) << std::endl;

		// resend message back
		send(clientSocket, buf, bytesRecv + 1, 0);
	}

	// 7. close socket
	close(clientSocket);





	return (0);
}