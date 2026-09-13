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


void Server::handlePollEvents(int listener, std::vector<struct pollfd>& pfds)
{
	for(size_t i = 0; i < pfds.size(); )
	{
		const int fd = pfds[i].fd;
		if (pfds[i].revents & (POLLIN | POLLHUP))
		{
			if (pfds[i].fd == listener)
				handleNewConnection(listener, pfds);
			else
					handleUpcomingData(pfds[i].fd);
		}
		if (i < pfds.size() && pfds[i].fd == fd)
			++i;
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
		std::string clientIP = inet_ntop2(clientAddr);
		Client *newClient = new Client(newFD, clientIP);
		_clients[newFD] = newClient;
		std::cout << "pollserver:newconnectionfrom " << clientIP << " " <<
		newFD << std::endl;
	}
}

// for testing purposes broadcast a message to all clients
void Server::broadcast(std::string &msg, int listener, int s, std::vector<struct pollfd>& pfds)
{
	for(size_t i = 0; i < pfds.size(); i++)
	{
		// checking if fd is included in master set
		if (pfds[i].fd != listener && pfds[i].fd != s)
		{
			if (sendall(pfds[i].fd, msg) == -1)
				throw std::runtime_error("Failed to send the data");
		}
	}
}

// Function that handles client data
void Server::handleUpcomingData(int fd)
{
	char	buf[4096];
	// int		nbytes;
	
	const ssize_t received = recv(fd, buf, sizeof(buf), 0);
	
	if (received == 0){
		quitClient(fd, "Connection closed");
		return;
	}
	if (received < 0){
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
            return;
        quitClient(fd, "Read error");
        return;
	}
	
	getClient(fd).appendInput(buf, received);
	std::string line;

	while (_clients.find(fd) != _clients.end()){
		Client &client = getClient(fd);

		if (!client.popLine(line)){
			if (client.inputSize() > 511)
				quitClient(fd, "Input line too long");
			return;
		}
		if (line.size() > 512){
            quitClient(fd, "Input line too long");
            return;
        }

		Parser parser;
        try{
			parser.parseGrammar(line);
        }
        catch (const std::exception &error){
            std::cerr << "Parser error: " << error.what() << std::endl;
            continue;
        }

        if (!parser.getCommand().empty())
            executeCommand(parser, fd);
	}
}

const std::string&	Server::getPassword(void) const
{
	return (_password);
}

void Server::sendMsgToChannel(Channel* ch, const std::string msg, int clientFd){
	const std::map<Client*,std::string> &members = ch->getMembers();

	for (std::map<Client*,std::string>::const_iterator it = members.begin(); it != members.end(); ++it){
		int recipientFD = (it->first)->getFD();
		if (clientFd == recipientFD)
			continue;
		else
			(it->first)->sendMsg(msg);
	}
}

// NICK helper
bool Server::nicknameExists(const std::string &nickname, int exceptFd) const
{
	for (std::map<int, Client*>::const_iterator it = _clients.begin(); it != _clients.end(); ++it)
	{
		if (it->first != exceptFd && it->second->getNickname() == nickname)
			return (true);
	}

	return (false);
}



struct pollfd *Server::findPollFD(int fd)
{
	for (size_t i = 0; i < _pollFDs.size(); ++i)
	{
		if (_pollFDs[i].fd == fd)
			return (&_pollFDs[i]);
	}

	return (NULL);
}

Client *Server::findClientByNickname(const std::string &nickname)
{
	for (std::map<int, Client*>::iterator it = _clients.begin();
		it != _clients.end(); ++it)
	{
		if (it->second->getNickname() == nickname)
			return it->second;
	}
	return NULL;
}

Client &Server::getClient(int clientFD)
{
	std::map<int, Client*>::iterator it = _clients.find(clientFD);

	if (it == _clients.end())
		throw std::runtime_error("Client not found");

	return (*(it->second));
}

const std::map<int, Client*>& Server::getClients() const{
	return _clients;
}

// Channel& Server::getChannel(const std::string &ch){

// 	std::map<std::string, Channel>::iterator it = _channels.find(ch);
	
// 	if (it == _channels.end())
// 		throw std::runtime_error("Channel not found");

// 	return it->second;
// }

std::map<std::string, Channel>&	Server::getChannels(){
	return _channels;
}

// =====================================
// server logic:
void	Server::executeCommand(Parser& parser, int clientFd)
{
	const std::string &command = parser.getCommand();	

	if (command == "PASS")
		handlePass(parser, clientFd);
	else if (command == "NICK")
		handleNick(parser, clientFd);
	else if (command == "USER")
		handleUser(parser, clientFd);
	else if (command == "JOIN")
		handleJoin(parser, clientFd);
	else if (command == "PRIVMSG")
		handlePrivmsg(parser, clientFd);
	else if (command == "PART")
		handlePart(parser, clientFd);
	else if (command == "KICK")
		handleKick(parser, clientFd);
	else if (command == "QUIT")
		handleQuit(parser, clientFd);
	else if (command == "TOPIC")
		handleTopic(parser, clientFd);
	else if (command == "INVITE")
		handleInvite(parser, clientFd);
	else if (command == "MODE")
		handleMode(parser, clientFd);
	else if (command == "AWAY")
		handleAway(parser, clientFd);
	// add more if more functions come
}

// PASS
void Server::handlePass(Parser& parser, int clientFd)
{
	const std::vector<std::string> &params = parser.getParams();
	Client &client = getClient(clientFd);

	if (params.empty())
	{
		client.sendMsg(":server 461 * PASS :Not enough parameters\r\n");
		return;
	}

	if (client.isAuth()) //is this check correct?
	{
		client.sendMsg(":server 462 * :Unauthorized command (already registered)\r\n");
		return;
	}

	if (params[0] != getPassword())
	{
		client.sendMsg(":server 464 * :Password incorrect\r\n");
		return;
	}

	client.setAuth(true);
}

// NICK
void Server::handleNick(Parser& parser, int clientFd)
{
	const std::vector<std::string> &params = parser.getParams();
	Client &client = getClient(clientFd);

	if (params.empty())
	{
		client.sendMsg(":server 431 * :No nickname given\r\n");
		return;
	}

	const std::string &nickname = params[0];

	if (nickname.length() > 9)
	{
		client.sendMsg(":server 432 * " + nickname + " :Erroneous nickname\r\n");
		return;
	}

	if (nicknameExists(nickname, clientFd))
	{
		client.sendMsg(":server 433 * " + nickname + " :Nickname is already in use\r\n");
		return;
	}

	client.setNickname(nickname);
}

// USER
void Server::handleUser(Parser& parser, int clientFd)
{
	const std::vector<std::string> &params = parser.getParams();
	const std::string &realname = parser.getTrailing();

	Client &client = getClient(clientFd);

	if (params.size() < 3 || realname.empty())
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

	client.setUsername(params[0]);
	client.setRealname(realname);
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
	std::map<std::string, Channel>::iterator it = _channels.find(channelName);

	if (it == _channels.end()){
		return NULL;
	} // change throw to some different return
		// throw std::runtime_error("Channel not found");

	return (&(it->second));
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
		// leaving all channels TODO
		return ;
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

		// check the channl name
		if (channelName.empty() || (channelName[0] != '#' && channelName[0] != '&'))
		{
			client.sendMsg(":server 403 " + client.getNickname()
				+ " " + channelName + " :No such channel\r\n");
			continue ;
		}

		// Key supplied for this channel, if any
		std::string providedKey;

		if (i < keys.size())
			providedKey = keys[i];

			
		Channel *channel = getChannel(channelName);

		// Channel does not exist -> create it
		if (channel == NULL)
		{
			_channels.insert(std::make_pair(channelName, Channel(channelName)));

			channel = getChannel(channelName);

			channel->addMember(&client, "operator");
			client.joinChannel(channelName);

			// First member should normally become channel operator
			// channel->addOperator(&client);

			// TODO:
			// broadcast JOIN
			// send topic
			// send NAMES

			// send information about all commands his server receives affecting the channel

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
		if (channel->isInviteOnlyMode() && !channel->isInvited(client.getNickname()))
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

		// successfull JOIN
		channel->addMember(&client, "user");
		client.joinChannel(channelName);

		// we need to remove invitation (it was used)
		channel->removeInvite(client.getNickname());

		// TODO:
		// broadcast JOIN
		// send topic
		// send NAMES
	}

}

// PRIVMSG

void Server::handlePrivmsg(Parser& parser, int clientFd){

	const std::vector<std::string> &params = parser.getParams();
	Client &client = getClient(clientFd);

	std::string msg = parser.getTrailing();
	
	if (params.empty() || msg.empty())
	{
		client.sendMsg(":server 461 " + client.getNickname() + " PRIVMSG :Not enough parameters\r\n");
    	return;
	}

	// std::string recipient = params[0];
	const std::set <std::string>& userChannels = client.getChannels();

	std::vector <std::string> channelsRec;
	std::vector <std::string> userRecipients;

	for (std::vector<std::string>::const_iterator it = params.begin(); it != params.end(); ++it){
		char firstChar = (*it)[0];
		if (firstChar != '#' && firstChar != '&' && firstChar != '+' && firstChar != '!')
			userRecipients.push_back(*it);
		else
			channelsRec.push_back(*it);
	}

	// SENDING MSG TO CHANNELS
	for (std::vector<std::string>::iterator it = channelsRec.begin(); it != channelsRec.end(); ++it){
		if (userChannels.find(*it) == userChannels.end()){
			client.sendMsg(":server 404 " + client.getNickname() + " " + *it + " :Cannot send to channel\r\n");
		}
		else{
			Channel *channel = getChannel(*it);
			if (NULL == channel){
				std::string formattedMsg = ":" + client.getNickname() + "!" + client.getUsername()
					+ "@" + client.getHostName() + " PRIVMSG " + *it + " :" + msg + "\r\n";
				sendMsgToChannel(channel, formattedMsg, clientFd);
				client.sendMsg(":server 403 " + client.getNickname() + " " + *it + " :No such channel\r\n");
			}
		}
	}

	// SENDING MSG TO USERS
	for (std::vector<std::string>::iterator it = userRecipients.begin(); it != userRecipients.end(); ++it){
		Client *target = findClientByNickname(*it);

		if (target == NULL)
			client.sendMsg(":server 401 " + client.getNickname() + " " + *it + " :No such nick/channel\r\n");
		else{
			std::string formattedMsg = ":" + client.getNickname() + "!" + client.getUsername()
				+ "@" + client.getHostName() + " PRIVMSG " + *it + " :" + msg + "\r\n";
			target->sendMsg(formattedMsg);
			if (target->isAway())
				client.sendMsg(":server 301 " + client.getNickname() + " " + target->getNickname()
					+ " :" + target->getAwayMessage() + "\r\n");
		}
	}
	}

void Server::handlePart(Parser& parser, int clientFd){

	const std::vector<std::string> &channels = parser.getParams();
	Client &client = getClient(clientFd);
	const std::set <std::string> userChannels = client.getChannels();
	const std::map <std::string, Channel> serverChannels = getChannels();
	std::string reason = parser.getTrailing();

	if (channels.empty()){
		client.sendMsg(":server 401 " + client.getNickname() + " :No channel specified\r\n");
		return;
	}


	for (std::vector<std::string>::const_iterator it = channels.begin(); it != channels.end(); it++){
		if (userChannels.find(*it) != userChannels.end()){
			Channel *channel = getChannel(*it);
			client.quitChannel(*it);
			channel->rmvMember(&client);

			std::string partMsg = ":" + client.getNickname() + " PART " + *it;
			if (!reason.empty()){
				partMsg += " :" + reason;
			}
			partMsg += ENDSIGN;
			sendMsgToChannel(channel, partMsg, clientFd);
		}
		else{
			client.sendMsg(":server 401 " + client.getNickname() + " " + *it + " :No such nick/channel\r\n");
			continue;
			
		}
	}
}

void Server::handleKick(Parser& parser, int clientFd)
{
	Client &client = getClient(clientFd);
	std::vector<std::string> params = parser.getParams();
	if (parser.hasTrailing())
		params.push_back(parser.getTrailing());
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
		target->quitChannel(channelName);
		channel->rmvMember(target);
		if (channel->getMembers().empty())
			_channels.erase(channelName);
	}
}

void Server::quitClient(int clientFd, const std::string &reason){
	std::map<int, Client*>::iterator found = _clients.find(clientFd);
	if (found == _clients.end())
		return;

	Client *client = found->second;
	const std::set<std::string> channels = client->getChannels();
	std::set<Client*> recipients;

	// A peer sharing several channels receives QUIT only once.
	for (std::set<std::string>::const_iterator it = channels.begin();
		it != channels.end(); ++it){
		Channel *channel = getChannel(*it);
		if (channel == NULL)
			continue;
		const std::map<Client*, std::string> &members = channel->getMembers();
		for (std::map<Client*, std::string>::const_iterator member = members.begin();
			member != members.end(); ++member){
			if (member->first != client)
				recipients.insert(member->first);
		}
	}

	const std::string message = ":" + client->getNickname() + "!"
		+ client->getUsername() + "@" + client->getHostname()
		+ " QUIT :" + reason + "\r\n";
	for (std::set<Client*>::const_iterator it = recipients.begin();
		it != recipients.end(); ++it)
		(*it)->sendMsg(message);

	for (std::set<std::string>::const_iterator it = channels.begin();
		it != channels.end(); ++it){
		Channel *channel = getChannel(*it);
		if (channel == NULL)
			continue;
		channel->rmvMember(client);
		if (channel->getMembers().empty())
			_channels.erase(*it);
	}

	rmvFromPollFDs(_pollFDs, clientFd);
	_clients.erase(found);
	delete client; // Client's destructor closes the socket.
}

void Server::handleQuit(Parser& parser, int clientFd){
	quitClient(clientFd, parser.hasTrailing() ? parser.getTrailing() : "Client Quit");
}

void Server::handleTopic(Parser& parser, int clientFd)
{
	Client &client = getClient(clientFd);
	std::vector<std::string> params = parser.getParams();
	if (parser.hasTrailing())
		params.push_back(parser.getTrailing());
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
	channel->setTopic(params[1]);
	const std::string msg = ":" + client.getNickname() + "!" + client.getUsername()
		+ "@" + client.getHostName() + " TOPIC " + channelName + " :" + params[1] + "\r\n";
	sendMsgToChannel(channel, msg, -1);
}

// INVITE
void Server::handleInvite(Parser& parser, int clientFd)
{
	Client &sender = getClient(clientFd);
	const std::vector<std::string> &params = parser.getParams();

	if (params.size() != 3)
	{
		// client.sendMsg(":server 461 " + client.getNickname() + " INVITE :Not enough parameters\r\n");
		sender.sendMsg(":server 461 INVITE :Not enough parameters\r\n");
		return ;
	}

	const std::string	&targetClientName = params[1]; //client nick
	const std::string	&channelName = params[2];

	// findClientByNickname
	Client *target = findClientByNickname(targetClientName);
	Channel *channel = getChannel(channelName);

	if (target == NULL)
	{
		// ERR_NOSUCHNICK
		sender.sendMsg(":server 401 " + targetClientName + " :No such nick/channel\r\n");
		return ;
	}
	if (channel == NULL)
	{
		// ERR_NOSUCHNICK
		sender.sendMsg(":server 401 " + channelName + " :No such nick/channel\r\n");
		return ;
	}

	if(!channel->isMember(sender))
	{
		// sender not in channel
		sender.sendMsg(":server 442 " + channelName + " :You're not on that channel\r\n");
		return ;
	}

	if(!channel->isMember(*target))
	{
		// sender not in channel
		sender.sendMsg(":server 443 " + targetClientName + " " + channelName + " :is already on channel\r\n");
		return ;
	}

	// now check the mode 'i'
	if (channel->isInviteOnlyMode() && !channel->memberIsOperator(sender))
	{
		sender.sendMsg(":server 482 " + channelName + " :You're not channel operator\r\n");
		return ;
	}

	channel->inviteUser(targetClientName);

	sender.sendMsg(":server 341 " + sender.getNickname() + " "
			+ channelName + " " + targetClientName + "\r\n");

	target->sendMsg(":" + sender.getNickname() + "!" + sender.getUsername()
			+ "@" + sender.getHostname() + " INVITE " + targetClientName + " "
			+ channelName + "\r\n");







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
	if (parser.hasTrailing())
		params.push_back(parser.getTrailing());
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
		if (user != client.getNickname()){
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
				if (mode == 'o' || mode == 'k' || (mode == 'l' && adding)){
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
					if (adding && (arg.empty() || arg.find_first_of(" \t\r\n\v\f,:\a") != std::string::npos || arg.find('\0') != std::string::npos)){
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
	else if (parser.hasTrailing())
		message = parser.getTrailing();

	client.setAwayMessage(message);
	client.setAway(!message.empty());
	if (client.isAway())
		client.sendMsg(":server 306 " + client.getNickname() + " :You have been marked as being away\r\n");
	else
		client.sendMsg(":server 305 " + client.getNickname() + " :You are no longer marked as being away\r\n");
}
