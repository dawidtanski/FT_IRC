#pragma once
#include "irc.hpp"
#include "utils.hpp"


class Server
{
	private:
		std::string					_name;
		int							_port;
		char						*_portStr; // temporary solution
		std::string					_password;
		int							_serverSocket;
		std::vector<struct pollfd>	_pollFDs;
		int							_pollCount;

		int							_status;
		struct addrinfo				_hints;
		struct addrinfo				*_servinfo;  // will point to the results

		void		createSocket();
		void		bindSocket();
		void		listenSocket();
		void		acceptClient();
		void		handleClient(int clientFd);
		void		handlePollEvents(int listener, std::vector<struct pollfd>& pfds);
		void		handleUpcomingData(int s, int listener, std::vector<struct pollfd>& pfds, int index);
		void		broadcast(std::string &msg, int listener, int s, std::vector<struct pollfd>& pfds);
		void		handleNewConnection(int listener, std::vector<struct pollfd>& pfds);



	public:
		// Server(void); // default constr?
		Server(int port, char *portStr, std::string password);

		void	start();
};
