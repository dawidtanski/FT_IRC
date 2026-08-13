#include "irc.hpp"


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
		void		addToPollFDs(std::vector<struct pollfd>& pfds, int newFD);
		void		handlePollEvents(int listener, std::vector<struct pollfd>& pfds);
		void		handleUpcomingData(int s, int listener, std::vector<struct pollfd>& pfds, int index);
		void		broadcast(char *buf, int nbytes, int listener, int s, std::vector<struct pollfd>& pfds);
		void		handleNewConnection(int listener, std::vector<struct pollfd>& pfds);
		int			sendall(int sock_fd, char *buf, int *len);
		std::string	inet_ntop2(const sockaddr_storage& addr);


	public:
		// Server(void); // default constr?
		Server(int port, char *portStr, std::string password);

		void	start();
};
