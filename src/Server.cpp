/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: kjamrosz <kjamrosz@student.42warsaw.pl>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/17 12:10:37 by kjamrosz          #+#    #+#             */
/*   Updated: 2026/09/17 12:12:12 by kjamrosz         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../include/Server.hpp"

namespace {
	volatile sig_atomic_t running = 1;
	void stopServer(int) { running = 0; }
	std::string prefix(const Client &client) {
		return ":" + client.getNickname() + "!" + client.getUsername()
			+ "@" + client.getHostname();
	}
	std::string replyTarget(const Client &client) {
		return client.getNickname().empty() ? "*" : client.getNickname();
	}
}

Server::Server(const char *port, const std::string &password)
	: _portStr(port), _password(password), _serverSocket(-1), _servinfo(NULL) {}

Server::~Server() {
	for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it)
		delete it->second;
	if (_serverSocket >= 0)
		close(_serverSocket);
	if (_servinfo)
		freeaddrinfo(_servinfo);
}

void Server::createSocket() {
	struct addrinfo hints;
	std::memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE | AI_NUMERICSERV;
	const int status = getaddrinfo(NULL, _portStr.c_str(), &hints, &_servinfo);
	if (status != 0)
		throw std::runtime_error(gai_strerror(status));
	for (struct addrinfo *addr = _servinfo; addr; addr = addr->ai_next) {
		_serverSocket = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
		if (_serverSocket < 0)
			continue;
		const int yes = 1;
		if (setsockopt(_serverSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == 0
			&& fcntl(_serverSocket, F_SETFL, O_NONBLOCK) != -1
			&& bind(_serverSocket, addr->ai_addr, addr->ai_addrlen) == 0
			&& listen(_serverSocket, SOMAXCONN) == 0)
			break;
		close(_serverSocket);
		_serverSocket = -1;
	}
	freeaddrinfo(_servinfo);
	_servinfo = NULL;
	if (_serverSocket < 0)
		throw std::runtime_error("Cannot bind/listen on the requested port");
}

void Server::start() {
	running = 1;
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR
		|| signal(SIGINT, stopServer) == SIG_ERR || signal(SIGTERM, stopServer) == SIG_ERR)
		throw std::runtime_error("Cannot install signal handlers");
	createSocket();
	addToPollFDs(_pollFDs, _serverSocket);
	std::cout << "IRC server listening on port " << _portStr << std::endl;
	while (running) {
		// Refresh interest before the single poll: no handler writes to a socket.
		for (size_t i = 1; i < _pollFDs.size();) {
			const int fd = _pollFDs[i].fd;
			Client &client = getClient(fd);
			if (client.outputFailed() || (client.isClosing() && !client.hasOutput())) {
				try { quitClient(fd, "Connection closed or output limit exceeded"); }
				catch (const std::bad_alloc &) { discardClient(fd); }
				continue;
			}
			_pollFDs[i].events = client.isClosing() ? 0 : POLLIN;
			if (client.hasOutput())
				_pollFDs[i].events |= POLLOUT;
			++i;
		}
		// Timeout also closes the small signal-before-poll race on shutdown.
		const int ready = poll(&_pollFDs[0], _pollFDs.size(), 1000);
		if (ready < 0) {
			if (errno == EINTR || errno == ENOMEM || errno == EAGAIN)
				continue;
			throw std::runtime_error("poll failed");
		}
		if (ready > 0)
			handlePollEvents();
	}
}

void Server::handlePollEvents() {
	for (size_t i = 0; i < _pollFDs.size();) {
		const int fd = _pollFDs[i].fd;
		const short events = _pollFDs[i].revents;
		try {
			if (fd == _serverSocket) {
				if (events & POLLIN)
					handleNewConnection();
			} else if (events & (POLLERR | POLLNVAL)) {
				quitClient(fd, "Socket error");
			} else {
				if ((events & POLLIN) && !getClient(fd).isClosing())
					handleUpcomingData(fd);
				if (_clients.find(fd) != _clients.end() && (events & POLLOUT)) {
					if (!getClient(fd).flushOutput())
						quitClient(fd, "Write error");
				}
				if (_clients.find(fd) != _clients.end() && (events & POLLHUP) && !(events & POLLIN))
					quitClient(fd, "Connection closed");
			}
		} catch (const std::bad_alloc &) {
			// No allocation is needed to detach a failed client from every channel.
			if (fd != _serverSocket)
				discardClient(fd);
		}
		if (i < _pollFDs.size() && _pollFDs[i].fd == fd)
			++i;
	}
}

void Server::handleNewConnection() {
	struct sockaddr_storage addr;
	socklen_t len = sizeof(addr);
	const int fd = accept(_serverSocket, reinterpret_cast<struct sockaddr*>(&addr), &len);
	if (fd < 0)
		return; // Transient failure or resource exhaustion must not stop existing clients.
	if (_clients.size() >= 1024 || fcntl(fd, F_SETFL, O_NONBLOCK) == -1) {
		close(fd);
		return;
	}
	Client *client = NULL;
	try {
		client = new Client(fd, inet_ntop2(addr));
		_clients.insert(std::make_pair(fd, client));
		addToPollFDs(_pollFDs, fd);
	} catch (const std::exception &) {
		_clients.erase(fd);
		if (client) delete client;
		else close(fd);
	}
}

void Server::handleUpcomingData(int fd) {
	char buf[4096];
	const ssize_t received = recv(fd, buf, sizeof(buf), 0);
	if (received <= 0) {
		if (received < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR))
			return;
		quitClient(fd, "Connection closed");
		return;
	}
	getClient(fd).appendInput(buf, static_cast<size_t>(received));
	std::string line;
	while (_clients.find(fd) != _clients.end()) {
		Client &client = getClient(fd);
		if (client.isClosing() || client.outputFailed())
			return;
		if (!client.popLine(line)) {
			if (client.inputSize() > 511)
				quitClient(fd, "Input line too long");
			return;
		}
		if (line.size() > 512) {
			quitClient(fd, "Input line too long");
			return;
		}
		Parser parser;
		try { parser.parseGrammar(line); }
		catch (const std::runtime_error &) {
			client.sendMsg(":server 417 " + replyTarget(client) + " :Malformed input\r\n");
			continue;
		}
		if (!parser.getCommand().empty())
			executeCommand(parser, fd);
	}
}

const std::string &Server::getPassword() const { return _password; }

void Server::sendMsgToChannel(Channel *ch, const std::string &msg, int exceptFd) {
	if (!ch) return;
	const std::map<Client*, std::string> &members = ch->getMembers();
	for (std::map<Client*, std::string>::const_iterator it = members.begin(); it != members.end(); ++it)
		if (it->first->getFD() != exceptFd)
			it->first->sendMsg(msg);
}

bool Server::nicknameExists(const std::string &nickname, int exceptFd) const {
	for (std::map<int, Client*>::const_iterator it = _clients.begin(); it != _clients.end(); ++it)
		if (it->first != exceptFd && ircCaseFold(it->second->getNickname()) == ircCaseFold(nickname))
			return true;
	return false;
}

Client *Server::findClientByNickname(const std::string &nickname) {
	for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it)
		if (it->second->isRegistered() && ircCaseFold(it->second->getNickname()) == ircCaseFold(nickname))
			return it->second;
	return NULL;
}

Client &Server::getClient(int fd) {
	std::map<int, Client*>::iterator it = _clients.find(fd);
	if (it == _clients.end()) throw std::runtime_error("Client not found");
	return *it->second;
}

void Server::executeCommand(Parser &parser, int fd) {
	Client &client = getClient(fd);
	const std::string &cmd = parser.getCommand();
	const std::vector<std::string> &args = parser.getParams();
	if (cmd == "CAP") { handleCap(parser, client); return; }
	if (cmd == "PING") {
		if (args.empty() || args[0].empty())
			client.sendMsg(":server 409 " + replyTarget(client) + " :No origin specified\r\n");
		else client.sendMsg(":server PONG server :" + args[0] + "\r\n");
		return;
	}
	if (cmd == "PONG") return;
	if (!client.isRegistered() && cmd != "PASS" && cmd != "NICK" && cmd != "USER" && cmd != "QUIT") {
		if (cmd != "NOTICE") client.sendMsg(":server 451 " + replyTarget(client) + " :You have not registered\r\n");
		return;
	}
	if (cmd == "PASS") handlePass(parser, fd);
	else if (cmd == "NICK") handleNick(parser, fd);
	else if (cmd == "USER") handleUser(parser, fd);
	else if (cmd == "JOIN") handleJoin(parser, fd);
	else if (cmd == "PRIVMSG" || cmd == "NOTICE") handlePrivmsg(parser, fd);
	else if (cmd == "PART") handlePart(parser, fd);
	else if (cmd == "KICK") handleKick(parser, fd);
	else if (cmd == "QUIT") handleQuit(parser, fd);
	else if (cmd == "TOPIC") handleTopic(parser, fd);
	else if (cmd == "INVITE") handleInvite(parser, fd);
	else if (cmd == "MODE") handleMode(parser, fd);
	else if (cmd == "AWAY") handleAway(parser, fd);
	else if (cmd == "MOTD") sendMotd(client);
	else if (cmd == "NAMES" || cmd == "WHO" || cmd == "WHOIS" || cmd == "LIST") handleQuery(parser, client);
	else client.sendMsg(":server 421 " + replyTarget(client) + " " + cmd + " :Unknown command\r\n");
}

void Server::handleCap(Parser &parser, Client &client) {
	const std::vector<std::string> &args = parser.getParams();
	if (args.empty()) {
		client.sendMsg(":server 461 " + replyTarget(client) + " CAP :Not enough parameters\r\n");
		return;
	}
	if (args[0] == "LS") {
		if (!client.isRegistered()) client.setCapNegotiating(true);
		client.sendMsg(":server CAP " + replyTarget(client) + " LS :\r\n");
	} else if (args[0] == "LIST") {
		client.sendMsg(":server CAP " + replyTarget(client) + " LIST :\r\n");
	} else if (args[0] == "REQ") {
		client.sendMsg(":server CAP " + replyTarget(client) + " NAK :" + (args.size() > 1 ? args[1] : "") + "\r\n");
	} else if (args[0] == "END") {
		client.setCapNegotiating(false);
		tryRegister(client);
	} else client.sendMsg(":server 410 " + replyTarget(client) + " " + args[0] + " :Invalid CAP subcommand\r\n");
}

void Server::sendMotd(Client &client) {
	const std::string nick = client.getNickname();
	client.sendMsg(":server 375 " + nick + " :- Message of the day -\r\n");
	client.sendMsg(":server 372 " + nick + " :- Welcome to ft_irc.\r\n");
	client.sendMsg(":server 376 " + nick + " :End of /MOTD command\r\n");
}

void Server::tryRegister(Client &client) {
	if (client.isRegistered() || client.isCapNegotiating() || !client.isAuth()
		|| client.getNickname().empty() || client.getUsername().empty()) return;
	client.setRegistered(true);
	const std::string nick = client.getNickname();
	client.sendMsg(":server 001 " + nick + " :Welcome to the IRC network " + prefix(client).substr(1) + "\r\n");
	client.sendMsg(":server 002 " + nick + " :Your host is server, running ft_irc-1.0\r\n");
	client.sendMsg(":server 003 " + nick + " :This server was created for the 42 ft_irc project\r\n");
	client.sendMsg(":server 004 " + nick + " server ft_irc-1.0 iw itkol\r\n");
	client.sendMsg(":server 005 " + nick + " CHANTYPES=#& PREFIX=(o)@ CHANMODES=,k,l,it CASEMAPPING=rfc1459 NICKLEN=9 CHANNELLEN=50 TOPICLEN=300 :are supported by this server\r\n");
	sendMotd(client);
}

void Server::handlePass(Parser &parser, int fd) {
	Client &client = getClient(fd);
	const std::vector<std::string> &args = parser.getParams();
	if (client.isRegistered()) {
		client.sendMsg(":server 462 " + replyTarget(client) + " :You may not reregister\r\n");
		return;
	}
	if (args.empty()) {
		client.sendMsg(":server 461 " + replyTarget(client) + " PASS :Not enough parameters\r\n");
		return;
	}
	if (args[0] != _password) {
		client.sendMsg(":server 464 " + replyTarget(client) + " :Password incorrect\r\n");
		client.closeAfterOutput();
		return;
	}
	client.setAuth(true);
	tryRegister(client);
}

void Server::handleNick(Parser &parser, int fd) {
	Client &client = getClient(fd);
	const std::vector<std::string> &args = parser.getParams();
	if (args.empty() || args[0].empty()) {
		client.sendMsg(":server 431 " + replyTarget(client) + " :No nickname given\r\n");
		return;
	}
	const std::string &nick = args[0];
	const std::string first = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ[]\\`_^{|}";
	if (nick.size() > 9 || first.find(nick[0]) == std::string::npos
		|| nick.find_first_not_of(first + "0123456789-") != std::string::npos) {
		client.sendMsg(":server 432 " + replyTarget(client) + " " + nick + " :Erroneous nickname\r\n");
		return;
	}
	if (nicknameExists(nick, fd)) {
		client.sendMsg(":server 433 " + replyTarget(client) + " " + nick + " :Nickname is already in use\r\n");
		return;
	}
	if (nick == client.getNickname()) return;
	if (client.isRegistered()) notifyPeers(client, prefix(client) + " NICK :" + nick + "\r\n", true);
	client.setNickname(nick);
	tryRegister(client);
}

void Server::handleUser(Parser &parser, int fd) {
	Client &client = getClient(fd);
	const std::vector<std::string> &args = parser.getParams();
	if (client.isRegistered() || !client.getUsername().empty()) {
		client.sendMsg(":server 462 " + replyTarget(client) + " :You may not reregister\r\n");
		return;
	}
	if (args.size() < 4 || args[0].empty() || args[3].empty()) {
		client.sendMsg(":server 461 " + replyTarget(client) + " USER :Not enough parameters\r\n");
		return;
	}
	if (args[0].find_first_of(" @!:\t") != std::string::npos) {
		client.sendMsg(":server 461 " + replyTarget(client) + " USER :Invalid username\r\n");
		return;
	}
	client.setUsername(args[0].substr(0, 20));
	client.setRealname(args[3].substr(0, 100));
	tryRegister(client);
}

// Join helper
static std::vector<std::string> splitParams(std::string str, char delimiter)
{
	std::stringstream test(str);
	std::string segment;
	std::vector<std::string> seglist;

	while(std::getline(test, segment, delimiter))
	{
		seglist.push_back(segment);
	}
	return (seglist);
}

// Join helper
Channel *Server::getChannel(std::string channelName)
{
	std::map<std::string, Channel>::iterator it = _channels.find(ircCaseFold(channelName));

	if (it == _channels.end()){
		return NULL;
	}

	return (&(it->second));
}

// JOIN replies
static void	sendTopic(Client &client, Channel *channel, std::string channelName)
{
	if (channel->getTopic().empty())
	{
		client.sendMsg(":server 331 " + client.getNickname() + " "
			+ channelName + " :No topic is set\r\n");
	}
	else
	{
		client.sendMsg(":server 332 " + client.getNickname() + " "
			+ channelName + " :" + channel->getTopic() + "\r\n");
	}
}

//Join helper
static void sendNames(Channel *channel, Client &client, std::string channelName) {
	const std::string base = ":server 353 " + client.getNickname() + " = " + channelName + " :";
	std::string names;
	const std::map<Client*, std::string> &members = channel->getMembers();
	for (std::map<Client*, std::string>::const_iterator it = members.begin(); it != members.end(); ++it) {
		const std::string name = (it->second == "operator" ? "@" : "") + it->first->getNickname();
		if (base.size() + names.size() + name.size() + 1 > 510) {
			client.sendMsg(base + names + "\r\n");
			names.clear();
		}
		if (!names.empty()) names += " ";
		names += name;
	}
	if (!names.empty()) client.sendMsg(base + names + "\r\n");
	client.sendMsg(":server 366 " + client.getNickname() + " " + channelName + " :End of /NAMES list\r\n");
}

// JOIN
void Server::handleJoin(Parser& parser, int clientFd)
{
	const std::vector<std::string> &params = parser.getParams();
	Client &client = getClient(clientFd);

	// we need at least 1 parameter
	if (params.empty())
	{
		client.sendMsg(":server 461 " + client.getNickname()
			+ " JOIN :Not enough parameters\r\n");
		return ;
	}

	// JOIN 0 - leave all channels
	if (params[0] == "0")
	{
		while (!client.getChannels().empty())
			partChannel(client, *client.getChannels().begin(), "Leaving all channels");
		return;
	}

	// >2 params - invalid
	if (params.size() > 2)	//no separate message for too many params
	{
		client.sendMsg(":server 461 " + client.getNickname()
			+ " JOIN :Not enough parameters\r\n");
		return ;
	}

	std::vector<std::string>	channels = splitParams(params[0], ',');
	std::vector<std::string>	keys;

	if (params.size() == 2)
		keys = splitParams(params[1], ',');

	for (size_t i = 0; i < channels.size(); ++i)
	{
		const std::string &channelName = channels[i];

		// Validate channel names before creating state.
		if (channelName.empty() || channelName.size() > 50
			|| channelName.find_first_of(" \a,:\t") != std::string::npos
			|| (channelName[0] != '#' && channelName[0] != '&'))
		{
			client.sendMsg(":server 403 " + client.getNickname()
				+ " " + channelName + " :No such channel\r\n");
			continue ;
		}

		if (client.getChannels().size() >= 50) {
			client.sendMsg(":server 405 " + client.getNickname() + " " + channelName + " :Too many channels\r\n");
			continue;
		}

		// Key supplied for this channel, if any
		std::string providedKey;

		if (i < keys.size())
			providedKey = keys[i];

			
		Channel *channel = getChannel(channelName);

			std::string msg = ":" + client.getNickname() + "!" + client.getUsername()
				+ "@" + client.getHostname() + " JOIN :" + channelName + "\r\n";

		// Channel does not exist -> create it
		if (channel == NULL)
		{
			_channels.insert(std::make_pair(ircCaseFold(channelName), Channel(channelName)));

			channel = getChannel(channelName);

			channel->addMember(&client, "operator");
			client.joinChannel(ircCaseFold(channelName));


			// broadcast JOIN
			sendMsgToChannel(channel, msg, -1);
			// send topic
			sendTopic(client, channel, channelName);
			// send NAMES
			sendNames(channel, client, channelName);


			continue ;
		}

		if (channel->isMember(client))
			continue;
		if (channel->getUserLimit() != 0
			&& channel->getMembers().size() >= channel->getUserLimit()){
			client.sendMsg(":server 471 " + client.getNickname() + " " + channelName + " :Cannot join channel (+l)\r\n");
			continue;
		}

		// Invite-only channel
		if (channel->isInviteOnlyMode() && !channel->isInvited(clientFd))
		{
			client.sendMsg(":server 473 " + client.getNickname() + " "
					+ channelName + " :Cannot join channel (+i)\r\n");

			continue;
		}

		// Channel exists and requires a key
		if (!channel->getKey().empty())
		{
			if (providedKey.empty() || providedKey != channel->getKey())
			{
				client.sendMsg(":server 475 " + client.getNickname()
					+ " " + channelName + " :Cannot join channel (+k)\r\n");
				continue ;
			}
		}

		// Successful JOIN
		channel->addMember(&client, "user");
		client.joinChannel(ircCaseFold(channelName));

		// we need to remove invitation (it was used)
		channel->removeInvite(clientFd);

		// broadcast JOIN
		sendMsgToChannel(channel, msg, -1);
		// send topic
		sendTopic(client, channel, channelName);
		// send NAMES
		sendNames(channel, client, channelName);
	}

}

// PRIVMSG

void Server::handlePrivmsg(Parser &parser, int fd) {
	Client &client = getClient(fd);
	const std::vector<std::string> &args = parser.getParams();
	const bool notice = parser.getCommand() == "NOTICE";
	if (args.empty() || args[0].empty()) {
		if (!notice) client.sendMsg(":server 411 " + client.getNickname() + " :No recipient given\r\n");
		return;
	}
	if (args.size() < 2 || args[1].empty()) {
		if (!notice) client.sendMsg(":server 412 " + client.getNickname() + " :No text to send\r\n");
		return;
	}
	const std::vector<std::string> targets = splitParams(args[0], ',');
	for (size_t i = 0; i < targets.size(); ++i) {
		const std::string &name = targets[i];
		if (name.empty()) continue;
		const std::string msg = prefix(client) + " " + parser.getCommand() + " " + name + " :" + args[1] + "\r\n";
		if (name[0] == '#' || name[0] == '&') {
			Channel *ch = getChannel(name);
			if (!ch) {
				if (!notice) client.sendMsg(":server 403 " + client.getNickname() + " " + name + " :No such channel\r\n");
			} else if (!ch->isMember(client)) {
				if (!notice) client.sendMsg(":server 404 " + client.getNickname() + " " + name + " :Cannot send to channel\r\n");
			} else sendMsgToChannel(ch, msg, fd);
		} else {
			Client *target = findClientByNickname(name);
			if (!target) {
				if (!notice) client.sendMsg(":server 401 " + client.getNickname() + " " + name + " :No such nick\r\n");
			} else {
				target->sendMsg(msg);
				if (!notice && target->isAway()) client.sendMsg(":server 301 " + client.getNickname() + " " + target->getNickname() + " :" + target->getAwayMessage() + "\r\n");
			}
		}
	}
}

void Server::partChannel(Client &client, const std::string &name, const std::string &reason) {
	Channel *ch = getChannel(name);
	if (!ch) {
		client.sendMsg(":server 403 " + client.getNickname() + " " + name + " :No such channel\r\n");
		return;
	}
	if (!ch->isMember(client)) {
		client.sendMsg(":server 442 " + client.getNickname() + " " + name + " :You're not on that channel\r\n");
		return;
	}
	const std::string key = ircCaseFold(name);
	sendMsgToChannel(ch, prefix(client) + " PART " + ch->getChannelName() + " :" + reason + "\r\n", -1);
	client.quitChannel(key);
	ch->rmvMember(&client);
	if (ch->getMembers().empty()) _channels.erase(key);
}

void Server::handlePart(Parser &parser, int fd) {
	Client &client = getClient(fd);
	const std::vector<std::string> &args = parser.getParams();
	if (args.empty() || args[0].empty()) {
		client.sendMsg(":server 461 " + client.getNickname() + " PART :Not enough parameters\r\n");
		return;
	}
	const std::vector<std::string> channels = splitParams(args[0], ',');
	for (size_t i = 0; i < channels.size(); ++i)
		partChannel(client, channels[i], args.size() > 1 ? args[1] : client.getNickname());
}

void Server::handleKick(Parser& parser, int clientFd)
{
	Client &client = getClient(clientFd);
	std::vector<std::string> params = parser.getParams();
	if (params.size() < 2 || params[0].empty() || params[1].empty()){
		client.sendMsg(":server 461 " + client.getNickname() + " KICK :Not enough parameters\r\n");
		return;
	}

	const std::vector<std::string> channels = splitParams(params[0], ',');
	const std::vector<std::string> targets = splitParams(params[1], ',');
	if (channels.empty() || targets.empty()
		|| (channels.size() != 1 && channels.size() != targets.size())){
		client.sendMsg(":server 461 " + client.getNickname() + " KICK :Invalid channel/user list\r\n");
		return;
	}
	const std::string reason = params.size() > 2 ? params[2] : client.getNickname();
	for (size_t i = 0; i < targets.size(); ++i){
		const std::string &channelName = channels[channels.size() == 1 ? 0 : i];
		Channel *channel = getChannel(channelName);
		if (!channel){
			client.sendMsg(":server 403 " + client.getNickname() + " " + channelName + " :No such channel\r\n");
			continue;
		}
		if (!channel->isMember(client)){
			client.sendMsg(":server 442 " + client.getNickname() + " " + channelName + " :You're not on that channel\r\n");
			continue;
		}
		if (!channel->memberIsOperator(client)){
			client.sendMsg(":server 482 " + client.getNickname() + " " + channelName + " :You're not channel operator\r\n");
			continue;
		}
		Client *target = findClientByNickname(targets[i]);
		if (!target){
			client.sendMsg(":server 401 " + client.getNickname() + " " + targets[i] + " :No such nick/channel\r\n");
			continue;
		}
		if (!channel->isMember(*target)){
			client.sendMsg(":server 441 " + client.getNickname() + " " + targets[i] + " " + channelName + " :They aren't on that channel\r\n");
			continue;
		}
		const std::string msg = ":" + client.getNickname() + "!" + client.getUsername()
			+ "@" + client.getHostName() + " KICK " + channelName + " " + targets[i] + " :" + reason + "\r\n";
		// Notify everyone, including the operator and target, before removal.
		sendMsgToChannel(channel, msg, -1);
		target->quitChannel(ircCaseFold(channelName));
		channel->rmvMember(target);
		if (channel->getMembers().empty())
			_channels.erase(ircCaseFold(channelName));
	}
}

void Server::notifyPeers(Client &client, const std::string &msg, bool includeSelf) {
	std::set<Client*> recipients;
	if (includeSelf) recipients.insert(&client);
	const std::set<std::string> &channels = client.getChannels();
	for (std::set<std::string>::const_iterator it = channels.begin(); it != channels.end(); ++it) {
		Channel *ch = getChannel(*it);
		if (!ch) continue;
		const std::map<Client*, std::string> &members = ch->getMembers();
		for (std::map<Client*, std::string>::const_iterator m = members.begin(); m != members.end(); ++m)
			if (m->first != &client) recipients.insert(m->first);
	}
	for (std::set<Client*>::iterator it = recipients.begin(); it != recipients.end(); ++it)
		(*it)->sendMsg(msg);
}

// Allocation-free cleanup also repairs partially completed JOINs on bad_alloc.
void Server::discardClient(int fd) {
	std::map<int, Client*>::iterator found = _clients.find(fd);
	if (found == _clients.end()) return;
	Client *client = found->second;
	for (std::map<std::string, Channel>::iterator it = _channels.begin(); it != _channels.end();) {
		it->second.rmvMember(client);
		it->second.removeInvite(fd);
		if (it->second.getMembers().empty()) _channels.erase(it++);
		else ++it;
	}
	rmvFromPollFDs(_pollFDs, fd);
	_clients.erase(found);
	delete client;
}

void Server::quitClient(int fd, const std::string &reason) {
	if (_clients.find(fd) == _clients.end()) return;
	Client &client = getClient(fd);
	notifyPeers(client, prefix(client) + " QUIT :" + reason + "\r\n", false);
	discardClient(fd);
}

void Server::handleQuit(Parser &parser, int fd) {
	quitClient(fd, parser.getParams().empty() ? "Client Quit" : parser.getParams()[0]);
}

void Server::handleTopic(Parser& parser, int clientFd)
{
	Client &client = getClient(clientFd);
	std::vector<std::string> params = parser.getParams();
	if (params.empty() || params[0].empty()){
		client.sendMsg(":server 461 " + client.getNickname() + " TOPIC :Not enough parameters\r\n");
		return;
	}
	const std::string &channelName = params[0];
	Channel *channel = getChannel(channelName);
	if (!channel){
		client.sendMsg(":server 403 " + client.getNickname() + " " + channelName + " :No such channel\r\n");
		return;
	}
	if (!channel->isMember(client)){
		client.sendMsg(":server 442 " + client.getNickname() + " " + channelName + " :You're not on that channel\r\n");
		return;
	}
	if (params.size() == 1){
		if (channel->getTopic().empty())
			client.sendMsg(":server 331 " + client.getNickname() + " " + channelName + " :No topic is set\r\n");
		else
			client.sendMsg(":server 332 " + client.getNickname() + " " + channelName + " :" + channel->getTopic() + "\r\n");
		return;
	}
	if (channel->isTopResMode() && !channel->memberIsOperator(client)){
		client.sendMsg(":server 482 " + client.getNickname() + " " + channelName + " :You're not channel operator\r\n");
		return;
	}
	channel->setTopic(params[1].substr(0, 300));
	const std::string msg = ":" + client.getNickname() + "!" + client.getUsername()
		+ "@" + client.getHostName() + " TOPIC " + channelName + " :" + channel->getTopic() + "\r\n";
	sendMsgToChannel(channel, msg, -1);
}

// INVITE
void Server::handleInvite(Parser &parser, int fd) {
	Client &sender = getClient(fd);
	const std::vector<std::string> &args = parser.getParams();
	const std::string nick = sender.getNickname();
	if (args.size() < 2 || args[0].empty() || args[1].empty()) {
		sender.sendMsg(":server 461 " + nick + " INVITE :Not enough parameters\r\n");
		return;
	}
	Client *target = findClientByNickname(args[0]);
	Channel *ch = getChannel(args[1]);
	if (!target) sender.sendMsg(":server 401 " + nick + " " + args[0] + " :No such nick\r\n");
	else if (!ch) sender.sendMsg(":server 403 " + nick + " " + args[1] + " :No such channel\r\n");
	else if (!ch->isMember(sender)) sender.sendMsg(":server 442 " + nick + " " + args[1] + " :You're not on that channel\r\n");
	else if (ch->isMember(*target)) sender.sendMsg(":server 443 " + nick + " " + args[0] + " " + args[1] + " :is already on channel\r\n");
	else if (ch->isInviteOnlyMode() && !ch->memberIsOperator(sender))
		sender.sendMsg(":server 482 " + nick + " " + args[1] + " :You're not channel operator\r\n");
	else {
		ch->inviteUser(target->getFD());
		sender.sendMsg(":server 341 " + nick + " " + target->getNickname() + " " + ch->getChannelName() + "\r\n");
		target->sendMsg(prefix(sender) + " INVITE " + target->getNickname() + " :" + ch->getChannelName() + "\r\n");
	}
}

void Server::handleClientMode(int clientFd, std::string word)
{
	Client &client = getClient(clientFd);
	if (word.size() != 2 || (word[0] != '+' && word[0] != '-')){
		client.sendMsg(":server 501 " + client.getNickname() + " :Unknown MODE flag\r\n");
		return;
	}
	const bool adding = word[0] == '+';
	bool changed = false;
	if (word[1] == 'o' || word[1] == 'O'){
		// Users cannot grant themselves server operator privileges.
		if (!adding){
			changed = client.isOperator();
			client.setOperator(false);
		}
	}
	else if (word[1] == 'i'){
		changed = client.isInvisible() != adding;
		client.setInvisible(adding);
	}
	else if (word[1] == 'w'){
		changed = client.isRecvWallops() != adding;
		client.setRecvWallops(adding);
	}
	else if (word[1] == 'r'){
		// A restricted user cannot remove their own restriction.
		if (adding){
			changed = !client.isRestricted();
			client.setRestricted(true);
		}
	}
	else if (word[1] == 's'){
		changed = client.isServNotices() != adding;
		client.setServNotices(adding);
	}
	else{
		client.sendMsg(":server 501 " + client.getNickname() + " :Unknown MODE flag\r\n");
		return;
	}
	if (changed)
		client.sendMsg(":" + client.getNickname() + "!" + client.getUsername()
			+ "@" + client.getHostName() + " MODE " + client.getNickname() + " " + word + "\r\n");
}

void Server::handleMode(Parser& parser, int clientFd){

	Client &client = getClient(clientFd);
	std::vector<std::string> params = parser.getParams();
	std::string channel,user;

	if (params.empty() || params[0].empty()){
		client.sendMsg(":server 461 " + client.getNickname() + " MODE :Not enough parameters\r\n");
		return;
	}

	if (!params[0].empty() && (params[0][0] == '#' || params[0][0] == '&'))
		channel = params[0];
	else
		user = params[0];
	
	// USER MODE
	if (!user.empty()){
		if (ircCaseFold(user) != ircCaseFold(client.getNickname())){
			client.sendMsg(":server 502 " + client.getNickname() + " :Cannot change mode for other users\r\n");
			return;
		}
		std::string modes = "+";
		// USER command with no params
		if (params.size() == 1){
			if (client.isInvisible())
    			modes += "i";
			if (client.isRecvWallops())
    			modes += "w";
			if (client.isOperator())
    			modes += "o";
			if (client.isRestricted())
        		modes += "r";
			if (client.isServNotices())
				modes += "s";		
			client.sendMsg(":server 221 " + client.getNickname() +
				" " + modes + "\r\n");
		}
		else{
			bool adding = true;
			for (size_t i = 1; i < params.size(); ++i){
				for (size_t j = 0; j < params[i].size(); ++j){
					const char mode = params[i][j];
					if (mode == '+' || mode == '-')
						adding = mode == '+';
					else
						handleClientMode(clientFd, std::string(adding ? "+" : "-") + mode);
				}
			}
			return ;
		}
	}
	// CHANNEL MODE
	else if (!channel.empty()){
		Channel *ch = getChannel(channel);
		if (!ch){
			client.sendMsg(":server 403 " + client.getNickname() + " " + channel + " :No such channel\r\n");
			return;
		}
		if (params.size() == 2 && (params[1] == "b" || params[1] == "+b")) {
			client.sendMsg(":server 368 " + client.getNickname() + " " + channel + " :End of channel ban list\r\n");
			return;
		}
		if (params.size() == 1){
			std::string modes = "+";
			std::string args;
			if (ch->isInviteOnlyMode())
				modes += "i";
			if (ch->isTopResMode())
				modes += "t";
			if (!ch->getKey().empty()){
				modes += "k";
				if (ch->isMember(client))
					args += " " + ch->getKey();
				else
					args += " *";
			}
			if (ch->getUserLimit() != 0){
				modes += "l";
				std::ostringstream limit;
				limit << ch->getUserLimit();
				args += " " + limit.str();
			}
			client.sendMsg(":server 324 " + client.getNickname() + " " + channel + " " + modes + args + "\r\n");
			return;
		}
		if (!ch->isMember(client)){
			client.sendMsg(":server 442 " + client.getNickname() + " " + channel + " :You're not on that channel\r\n");
			return;
		}
		if (!ch->memberIsOperator(client)){
			client.sendMsg(":server 482 " + client.getNickname() + " " + channel + " :You're not channel operator\r\n");
			return;
		}
		bool adding = true;
		size_t paramIndex = 1;
		while (paramIndex < params.size()){
			const std::string modes = params[paramIndex++];
			for (size_t i = 0; i < modes.size(); ++i){
				char mode = modes[i];
				if (mode == '+' || mode == '-'){
					adding = (mode == '+');
					continue;
				}
				if (mode != 'i' && mode != 't' && mode != 'k' && mode != 'o' && mode != 'l'){
					client.sendMsg(":server 472 " + client.getNickname() + " " + mode + " :is unknown mode char to me\r\n");
					continue;
				}
				std::string arg;
				if (mode == 'o' || (mode == 'k' && adding) || (mode == 'l' && adding)){
					if (paramIndex >= params.size()){
						client.sendMsg(":server 461 " + client.getNickname() + " MODE :Not enough parameters\r\n");
						break;
					}
					arg = params[paramIndex++];
				}
				if (mode == 'i')
					ch->setInviteOnly(adding);
				else if (mode == 't')
					ch->setTopicRestricted(adding);
				else if (mode == 'k'){
					if (!adding && paramIndex < params.size()) {
						// Accept an optional old key without consuming later mode arguments.
						size_t needed = 0;
						bool nextAdding = adding;
						for (size_t j = i + 1; j < modes.size(); ++j) {
							if (modes[j] == '+' || modes[j] == '-') nextAdding = modes[j] == '+';
							else if (modes[j] == 'o' || (nextAdding && (modes[j] == 'k' || modes[j] == 'l'))) ++needed;
						}
						if (params.size() - paramIndex > needed && !params[paramIndex].empty()
							&& params[paramIndex][0] != '+' && params[paramIndex][0] != '-')
							arg = params[paramIndex++];
					}
					if (adding && (arg.empty() || arg.size() > 50 || arg.find_first_of(" \t\r\n\v\f,:\a") != std::string::npos || arg.find('\0') != std::string::npos)){
						client.sendMsg(":server 696 " + client.getNickname() + " " + channel + " k * :Invalid key\r\n");
						continue;
					}
					ch->setKey(adding ? arg : "");
				}
				else if (mode == 'l'){
					size_t limit = 0;
					if (adding){
						std::istringstream value(arg);
						if (arg.empty() || arg.find_first_not_of("0123456789") != std::string::npos
							|| !(value >> limit) || limit == 0){
							client.sendMsg(":server 696 " + client.getNickname() + " " + channel + " l " + arg + " :Invalid limit\r\n");
							continue;
						}
					}
					ch->setUserLimit(limit);
				}
				else if (mode == 'o'){
					Client *target = findClientByNickname(arg);
					if (!target){
						client.sendMsg(":server 401 " + client.getNickname() + " " + arg + " :No such nick/channel\r\n");
						continue;
					}
					if (!ch->isMember(*target)){
						client.sendMsg(":server 441 " + client.getNickname() + " " + arg + " " + channel + " :They aren't on that channel\r\n");
						continue;
					}
					ch->changeMemberMode(target, adding ? "operator" : "user");
				}
				std::string msg = ":" + client.getNickname() + "!" + client.getUsername()
					+ "@" + client.getHostName() + " MODE " + channel + " " + (adding ? "+" : "-") + mode;
				if (!arg.empty())
					msg += " " + arg;
				sendMsgToChannel(ch, msg + "\r\n", -1);
			}
		}
	}

}

// AWAY
void Server::handleAway(Parser& parser, int clientFd){

	Client &client = getClient(clientFd);
	const std::vector<std::string> &params = parser.getParams();
	std::string message;

	if (!params.empty())
		message = params[0];

	client.setAwayMessage(message);
	client.setAway(!message.empty());
	if (client.isAway())
		client.sendMsg(":server 306 " + client.getNickname() + " :You have been marked as being away\r\n");
	else
		client.sendMsg(":server 305 " + client.getNickname() + " :You are no longer marked as being away\r\n");
}

// Queries sent by real IRC clients when opening a channel or user window.
void Server::handleQuery(Parser &parser, Client &client) {
	const std::vector<std::string> &args = parser.getParams();
	const std::string &cmd = parser.getCommand();
	const std::string nick = client.getNickname();
	if (cmd == "NAMES") {
		if (args.empty()) {
			for (std::map<std::string, Channel>::iterator it = _channels.begin(); it != _channels.end(); ++it)
				if (it->second.isMember(client)) sendNames(&it->second, client, it->second.getChannelName());
			client.sendMsg(":server 366 " + nick + " * :End of /NAMES list\r\n");
		} else {
			const std::vector<std::string> names = splitParams(args[0], ',');
			for (size_t i = 0; i < names.size(); ++i) {
				Channel *ch = getChannel(names[i]);
				if (ch) sendNames(ch, client, ch->getChannelName());
				else client.sendMsg(":server 366 " + nick + " " + names[i] + " :End of /NAMES list\r\n");
			}
		}
	} else if (cmd == "WHO") {
		const std::string mask = args.empty() || args[0].empty() ? "*" : args[0];
		Channel *ch = getChannel(mask);
		if (ch) {
			const std::map<Client*, std::string> &members = ch->getMembers();
			for (std::map<Client*, std::string>::const_iterator it = members.begin(); it != members.end(); ++it) {
				Client &target = *it->first;
				if (target.isInvisible() && !ch->isMember(client)) continue;
				client.sendMsg(":server 352 " + nick + " " + ch->getChannelName() + " " + target.getUsername()
					+ " " + target.getHostname() + " server " + target.getNickname() + " "
					+ (target.isAway() ? "G" : "H") + (it->second == "operator" ? "@" : "")
					+ " :0 " + target.getRealname() + "\r\n");
			}
		}
		client.sendMsg(":server 315 " + nick + " " + mask + " :End of /WHO list\r\n");
	} else if (cmd == "WHOIS") {
		if (args.empty() || args[0].empty()) {
			client.sendMsg(":server 431 " + nick + " :No nickname given\r\n");
			return;
		}
		Client *target = findClientByNickname(args[0]);
		if (!target) client.sendMsg(":server 401 " + nick + " " + args[0] + " :No such nick\r\n");
		else {
			client.sendMsg(":server 311 " + nick + " " + target->getNickname() + " " + target->getUsername()
				+ " " + target->getHostname() + " * :" + target->getRealname() + "\r\n");
			client.sendMsg(":server 312 " + nick + " " + target->getNickname() + " server :ft_irc\r\n");
		}
		client.sendMsg(":server 318 " + nick + " " + args[0] + " :End of /WHOIS list\r\n");
	} else if (cmd == "LIST") {
		client.sendMsg(":server 321 " + nick + " Channel :Users Name\r\n");
		const std::vector<std::string> names = args.empty() ? std::vector<std::string>() : splitParams(args[0], ',');
		for (std::map<std::string, Channel>::iterator it = _channels.begin(); it != _channels.end(); ++it) {
			bool show = args.empty();
			for (size_t i = 0; i < names.size(); ++i)
				if (ircCaseFold(names[i]) == it->first) show = true;
			if (!show) continue;
			std::ostringstream count;
			count << it->second.getMembers().size();
			client.sendMsg(":server 322 " + nick + " " + it->second.getChannelName() + " " + count.str()
				+ " :" + it->second.getTopic() + "\r\n");
		}
		client.sendMsg(":server 323 " + nick + " :End of /LIST\r\n");
	}
}
