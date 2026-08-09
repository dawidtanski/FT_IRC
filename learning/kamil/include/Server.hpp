#include "irc.hpp"


class Server
{
	private:
		std::string	_name;
		int			_port;
		std::string	_password;
		int			_serverFd;

		void	createSocket();
		void	bindSocket();
		void	listenSocket();
		void	acceptClient();
		void	handleClient(int clientFd);

	public:
		// Server(void); // default constr?
		Server(int port, std::string password);

		void	start();
};