#pragma once
#include "irc.hpp"
#include "Channel.hpp"
#include "Client.hpp"
#include "Parser.hpp"

class Server
{
	private:
		std::string _portStr;
		std::string _password;
		int _serverSocket;
		std::vector<struct pollfd> _pollFDs;
		struct addrinfo *_servinfo;
		std::map<std::string, Channel> _channels;
		std::map<int, Client*> _clients;

		Server(const Server &);
		Server &operator=(const Server &);
		void createSocket();
		void handlePollEvents();
		void handleUpcomingData(int fd);
		void handleNewConnection();
		void discardClient(int fd);
		void quitClient(int fd, const std::string &reason = "Connection closed");
		void notifyPeers(Client &client, const std::string &message, bool includeSelf);
		void partChannel(Client &client, const std::string &name, const std::string &reason);
		void handleCap(Parser &parser, Client &client);
		void handleQuery(Parser &parser, Client &client);
		void sendMotd(Client &client);

	public:
		Server(const char *port, const std::string &password);
		~Server();
		void start();
		Client &getClient(int fd);
		Client *findClientByNickname(const std::string &nickname);
		const std::string &getPassword() const;
		Channel *getChannel(std::string name);
		void sendMsgToChannel(Channel *channel, const std::string &message, int exceptFd);
		bool nicknameExists(const std::string &nickname, int exceptFd) const;
		void tryRegister(Client &client);
		void executeCommand(Parser &parser, int fd);
		void handlePass(Parser &parser, int fd);
		void handleNick(Parser &parser, int fd);
		void handleUser(Parser &parser, int fd);
		void handleJoin(Parser &parser, int fd);
		void handlePrivmsg(Parser &parser, int fd);
		void handlePart(Parser &parser, int fd);
		void handleKick(Parser &parser, int fd);
		void handleQuit(Parser &parser, int fd);
		void handleTopic(Parser &parser, int fd);
		void handleInvite(Parser &parser, int fd);
		void handleMode(Parser &parser, int fd);
		void handleClientMode(int fd, std::string word);
		void handleAway(Parser &parser, int fd);
};
