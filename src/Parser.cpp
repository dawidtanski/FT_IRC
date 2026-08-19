#include "../include/Parser.hpp"

bool Parser::commandCheck(const std::string &cmd){
	
	if (cmd.compare("KICK") == 0 || cmd.compare("INVITE") == 0 
	|| cmd.compare("TOPIC") == 0 || cmd.compare("MODE") == 0
	|| cmd.compare("PASS") == 0 || cmd.compare("JOIN") == 0
	|| cmd.compare("NICK") == 0 || cmd.compare("USER") == 0
	|| cmd.compare("PRIVMSG") == 0 || cmd.compare("PART") == 0
	|| cmd.compare("QUIT") == 0)
		return true;
	return false;
}

// Parser use grammar from RFC 2812 standard
// [ ":" prefix SPACE ] command [ params ] crlf
void Parser::parseGrammar(const std::string &msgIRC){

	_prefix.clear();
    _command.clear();
    _params.clear();
    _trailing.clear();

	std::string endSign = "\r\n";
	std::string msg = msgIRC;
	// According to RFC2812 empty messages are silently ignored
	if (msg.empty() || onlyWhitespace(msg))
		return;
	if (msg.size() < 2 || msg.substr((msg.size() - 2), 2) != endSign)
		throw std::runtime_error("Invalid IRC message: message not terminated with CRLF");
	// parsing prefix
	if (msgIRC.at(0) == ':'){
		size_t end = msg.find(' ');
		if (end == std::string::npos)
			throw std::runtime_error("Invalid IRC message: malformed prefix");
		_prefix = msg.substr(1, end - 1);
		msg.erase(0, end + 1);
	}
	// parsing command
	size_t end = findTokenEnd(msg, endSign);
	if (end == 0)
		throw std::runtime_error("Invalid IRC message: missing command");
	// size_t end = msg.find(' ') || msg.find(endSign);
	if (commandCheck(msg.substr(0, end)) == 0)
		throw std::runtime_error("Invalid IRC message: unsupported command");
	_command = msg.substr(0, end);
	msg.erase(0, end);
	// Only 1 trailing param
	if (msg.compare(endSign) == 0)
		return ;
	if (!msg.empty() && msg[0] == ' ')
		msg.erase(0, 1);	
	// parsing middle params and trailing param
	while (msg.size() > 2){
		if (msg.at(0) == ':'){
			size_t end = msg.find(endSign);
			if (end == std::string::npos)
				throw std::runtime_error("Invalid IRC message: missing CRLF");
			if (end + endSign.size() != msg.size())
				throw std::runtime_error("Invalid IRC message: data after trailing parameter");
			_trailing = msg.substr(1, end - 1);
			return;
		}
		else{
			size_t end = findTokenEnd(msg, endSign);
			if (end == 0)
				throw std::runtime_error("Invalid IRC message: empty parameter");
			_params.push_back(msg.substr(0, end));
			if (_params.size() > 14)
				throw std::runtime_error("Invalid IRC message: too many params. Max 14 allowed");
			// CRLF found -> no more parameters
			if (msg.compare(end, endSign.size(), endSign) == 0)
				return;
			msg.erase(0, end + 1);
		}
	}	
}

// server logic:
// PASS
void Parser::handlePass(Server &server, int clientFd)
{
	Client &client = server.getClient(clientFd);

	if (_params.empty())
	{
		client.sendMsg(":server 461 * PASS :Not enough parameters\r\n");
		return;
	}

	if (client.isAuth())
	{
		client.sendMsg(":server 462 * :Unauthorized command (already registered)\r\n");
		return;
	}

	if (_params[0] != server.getPassword())
	{
		client.sendMsg(":server 464 * :Password incorrect\r\n");
		return;
	}

	client.setAuth(true);
}

// NICK
void Parser::handleNick(Server &server, int clientFd)
{
	Client &client = server.getClient(clientFd);

	if (_params.empty())
	{
		client.sendMsg(":server 431 * :No nickname given\r\n");
		return;
	}

	const std::string &nickname = _params[0];

	if (nickname.length() > 9)
	{
		client.sendMsg(":server 432 * " + nickname + " :Erroneous nickname\r\n");
		return;
	}

	if (server.nicknameExists(nickname, clientFd))
	{
		client.sendMsg(":server 433 * " + nickname + " :Nickname is already in use\r\n");
		return;
	}

	client.setNickname(nickname);
}

// USER
void Parser::handleUser(Server &server, int clientFd)
{
	Client &client = server.getClient(clientFd);

	if (_params.size() < 3 || _trailing.empty())
	{
		std::string nick = client.getNickname();

		if (nick.empty())
			nick = "*";

		client.sendMsg(":server 461 " + nick + " USER :Not enough parameters\r\n");
		return;
	}

	if (!client.getUsername().empty())
	{
		client.sendMsg(":server 462 " + client.getNickname() + " :Unauthorized command (already registered)\r\n");
		return;
	}

	client.setUsername(_params[0]);
	client.setRealname(_trailing);
}
