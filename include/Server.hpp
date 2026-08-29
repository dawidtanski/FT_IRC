#pragma once
#include "irc.hpp"
#include "utils.hpp"
#include "Channel.hpp"
#include "Client.hpp"
#include "Parser.hpp"

class Server
{
	private:
		std::string						_name;
		int								_port;
		char							*_portStr; // temporary solution
		std::string						_password;
		int								_serverSocket;
		std::vector<struct pollfd>		_pollFDs;
		int								_pollCount;

		int								_status;
		struct addrinfo					_hints;
		struct addrinfo					*_servinfo;  // will point to the results

		std::map<std::string, Channel>	_channels;
		std::map<int, Client*>			_clients;

		void		createSocket();
		void		bindSocket();
		void		listenSocket();
		// void		acceptClient();
		// void		handleClient(int clientFd);
		void		handlePollEvents(int listener, std::vector<struct pollfd>& pfds);
		void		handleUpcomingData(int s, int listener, std::vector<struct pollfd>& pfds, int index);
		void		broadcast(std::string &msg, int listener, int s, std::vector<struct pollfd>& pfds);
		void		handleNewConnection(int listener, std::vector<struct pollfd>& pfds);
		// SOME EXTRACT MESSAGES FUNCTION TO DIVIDE DATA FROM DCP TO MESSAGES BY CRLF

	public:
		// Server(void); // default constr?
		Server(int port, char *portStr, std::string password);

		void	start();

		Client			&getClient(int clientFD);
		const std::map<int, Client*> &getClients() const;
		struct pollfd	*findPollFD(int fd);
		Client *findClientByNickname(const std::string &nickname);
		const std::string&		getPassword(void) const;
		Channel &getChannel(const std::string &ch);
		std::map<std::string, Channel>	&getChannels();

		void sendMsgToChannel(Channel* ch, const std::string msg, int clientFd);

		bool		nicknameExists(const std::string &nickname, int exceptFd) const;

		// SERVER LOGIC
		void executeCommand(Parser& parser, int clientFd);

		void handlePass(Parser& parser, int clientFd);
		void handleNick(Parser& parser, int clientFd);
		void handleUser(Parser& parser, int clientFd);
		void handlePrivmsg(Parser& parser, int clientFd);
		void handlePart(Parser& parser, int clientFd);

		

		// TODO:
		void handleJoin();
		void handleQuit();
		void handleMode();
		void handleKick();
		void handleInvite();
		void handleTopic();

};
