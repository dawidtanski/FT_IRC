#pragma once
#include "irc.hpp"
#include "Server.hpp"

class Parser{

	private:
		std::string _prefix;
		std::string _command;
		std::vector<std::string> _params;
		std::string _trailing;
		bool commandCheck(const std::string &cmd);

	public:
		void parseGrammar(const std::string &msgIRC);
		void executeCommand(Server& server);

		void handlePass(Server &server, int clientFd);
		void handleNick(Server &server, int clientFd);
		void handleUser(Server &server, int clientFd);

		// TODO:
		void handleJoin(Server& server);
		void handlePrivmsg(Server& server);
		void handlePart(Server& server);
		void handleQuit(Server& server);
		void handleMode(Server& server);
		void handleKick(Server& server);
		void handleInvite(Server& server);
		void handleTopic(Server& server);
};
