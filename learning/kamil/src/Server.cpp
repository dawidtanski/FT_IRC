#include "../include/Server.hpp"

// CONSTRUCTORS

Server::Server(int port, std::string password) : _port(port), _password(password)
{
}

// FUNCTIONS

void Server::start()
{
	createSocket();
	bindSocket();
	listenSocket();

	while (true)
	{
		acceptClient();
	}
}

void Server::createSocket()
{
	_serverFd = socket(AF_INET, SOCK_STREAM, 0);

	if (_serverFd == -1)
		throw std::runtime_error("socket() failed");
}

void Server::bindSocket()
{
	sockaddr_in addr;

	addr.sin_family = AF_INET;
	addr.sin_port = htons(_port);
	addr.sin_addr.s_addr = INADDR_ANY;

	if (bind(_serverFd, (sockaddr *)&addr, sizeof(addr)) == -1)
	{
		throw std::runtime_error("bind() failed");
	}
}

void Server::listenSocket()
{
	if (listen(_serverFd, SOMAXCONN) == -1)
		throw std::runtime_error("listen() failed");

	std::cout << "Waiting for clients..." << std::endl;
}

void Server::acceptClient()
{
	sockaddr_in clientAddr;
	socklen_t len = sizeof(clientAddr);

	int clientFd = accept(_serverFd, (sockaddr *)&clientAddr, &len);

	if (clientFd == -1)
		return;

	std::cout << "Client connected!" << std::endl;

	handleClient(clientFd);
}

void Server::handleClient(int clientFd)
{
	char buffer[512];

	while (true)
	{
		int bytes = recv(clientFd, buffer, sizeof(buffer) - 1, 0);

		if (bytes <= 0)
			break;

		buffer[bytes] = '\0';

		std::cout << buffer;
	}

	close(clientFd);

	std::cout << "Client disconnected." << std::endl;
}

