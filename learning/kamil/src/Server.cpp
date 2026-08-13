#include "../include/Server.hpp"

// CONSTRUCTORS

Server::Server(int port, char *portStr, std::string password) : _port(port), _portStr(portStr), _password(password)
{
}

// FUNCTIONS

void Server::start()
{
	createSocket();
	bindSocket();
	listenSocket();

	// add server socket to the set of pollFDs
	addToPollFDs(_pollFDs, _serverSocket);

	while (true)
	{
		_pollCount = poll(_pollFDs.data(), _pollFDs.size(), -1);
		
		if (_pollCount == -1)
			throw std::runtime_error("poll() failed");
		handlePollEvents(_serverSocket, _pollFDs);
	}
	freeaddrinfo(this->_servinfo);
}

void Server::createSocket()
{
	memset(&this->_hints, 0, sizeof _hints); // make sure the struct is empty
	_hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
	_hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
	_hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

	// previously i used 'reinterpret_cast<const char*>_port' but it gave segfault
	if ((_status = getaddrinfo(NULL, _portStr, &_hints, &_servinfo)) != 0)
	{
		throw std::runtime_error(gai_strerror(_status));
		exit(1);
	}

	_serverSocket = socket(_servinfo->ai_family, _servinfo->ai_socktype, _servinfo->ai_protocol);

	if (_serverSocket == -1)
		throw std::runtime_error("socket() failed");
}

void Server::bindSocket()
{
	if (bind(_serverSocket, _servinfo->ai_addr, _servinfo->ai_addrlen) == -1)
		throw std::runtime_error("bind() failed");
}

void Server::listenSocket()
{
	if (listen(_serverSocket, SOMAXCONN) == -1)
		throw std::runtime_error("listen() failed");

	std::cout << "Waiting for clients..." << std::endl;
}

// ==========================

void Server::addToPollFDs(std::vector<struct pollfd>& pfds, int newFD)
{
	struct pollfd	pfd;

	pfd.fd = newFD;
	pfd.events = POLLIN;
	pfd.revents = 0;

	pfds.push_back(pfd);
}

void Server::handlePollEvents(int listener, std::vector<struct pollfd>& pfds)
{
	for(size_t i = 0; i < pfds.size(); i++)
	{
		if (pfds[i].revents & (POLLIN | POLLHUP))
		{
			if (pfds[i].fd == listener)
				handleNewConnection(listener, pfds);
			else
				handleUpcomingData(pfds[i].fd, listener, pfds, i);
		}
	}
}

void Server::handleNewConnection(int listener, std::vector<struct pollfd>& pfds)
{
	socklen_t				addrLen;
	int						newFD;
	struct sockaddr_storage	clientAddr;
	// char clientIP[INET6_ADDRSTRLEN];

	addrLen = sizeof(clientAddr);
	newFD = accept(listener, (struct sockaddr *)&clientAddr, &addrLen);
	if (newFD == -1)
		throw std::runtime_error("Failed to accept the conneciton");
	else
	{
		addToPollFDs(pfds, newFD);
		std::cout << "pollserver:newconnectionfrom " << inet_ntop2(clientAddr) <<
		newFD << std::endl;
	}
}

// for testing purposes broadcast a message to all clients
void Server::broadcast(char *buf, int nbytes, int listener, int s, std::vector<struct pollfd>& pfds)
{
	for(size_t i = 0; i < pfds.size(); i++)
	{
		// checking if fd is included in master set
		if (pfds[i].fd != listener && pfds[i].fd != s)
		{
			int bytesToSend = nbytes;
			if (sendall(pfds[i].fd, buf, &bytesToSend) == -1)
				throw std::runtime_error("Failed to send the data");
		}
	}
}

// Function that handles client data
void Server::handleUpcomingData(int s, int listener, std::vector<struct pollfd>& pfds, int index)
{
	char	buf[256];
	int		nbytes;

	if ((nbytes = recv(s, buf, sizeof buf, 0)) <= 0)
	{
		if (nbytes == 0)
			std::cout << "Socket " << s << " hung up." << std::endl;
		else
			throw std::runtime_error("Failed to receive data from client");
		close(s);
		pfds.erase(pfds.begin() + index); 
	}
	else
	{
		broadcast(buf, nbytes, listener, s, pfds);
	}
}

int Server::sendall(int sock_fd, char *buf, int *len)
{
	int total = 0;
	int bytesLeft = *len;
	int n = 0;

	while (total < *len)
	{
		n = send(sock_fd, buf+total, bytesLeft, 0);
		if (n == -1)
			break;
		total += n;
		bytesLeft -= n;
	}
	*len = total;

	return (n == -1 ? -1 : 0);
}


//inet_ntop(AF_INET, &(sa.sin_addr), ip4, INET_ADDRSTRLEN);
// Function that converts ip from "network to presentation"
std::string Server::inet_ntop2(const sockaddr_storage& addr)
{
	char buf[INET6_ADDRSTRLEN];

	if (addr.ss_family == AF_INET)
	{
		const sockaddr_in *sa4 = (const sockaddr_in *)&addr;
		if (!inet_ntop(sa4->sin_family, &sa4->sin_addr, buf, INET6_ADDRSTRLEN))
			throw std::runtime_error("inet_ntop failed");
		return buf;
	}
	else if (addr.ss_family == AF_INET6)
	{
		const sockaddr_in6 *sa6 = (const sockaddr_in6 *)&addr;
		if(!inet_ntop(sa6->sin6_family, &sa6->sin6_addr, buf, INET6_ADDRSTRLEN))
			throw std::runtime_error("inet_ntop failed");
		return buf;
	}
	else
		throw std::runtime_error("Couldn't convert address from binary to string");
}
