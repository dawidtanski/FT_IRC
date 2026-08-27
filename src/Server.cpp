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
	for(size_t i = 0; i < pfds.size(); i++)
	{
		if (pfds[i].revents & (POLLIN | POLLHUP))
		{
			if (pfds[i].fd == listener)
				handleNewConnection(listener, pfds);
			else
				handleUpcomingData(pfds[i].fd, listener, pfds, i);
		}
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
		std::cout << "pollserver:newconnectionfrom " << inet_ntop2(clientAddr) <<
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
void Server::handleUpcomingData(int s, int listener, std::vector<struct pollfd>& pfds, int index)
{
	char	buf[256];
	int		nbytes;
	
	nbytes = recv(s, buf, sizeof(buf), 0);
	
	if (nbytes <= 0)
	{
		if (nbytes == 0)
			std::cout << "Socket " << s << " hung up." << std::endl;
		else
			std::cerr << "Failed to receive data from client" << std::endl;

		close(s);
		pfds.erase(pfds.begin() + index);

		return ;
	}

	std::string msg(buf, nbytes);

	Parser parser;

	try
	{
		parser.parseGrammar(msg);
		executeCommand(parser, s);
	}
	catch (const std::exception &e)
	{
		std::cerr << "Parser error: " << e.what() << std::endl;
	}
}

std::string	Server::getPassword(void)
{
	return (_password);
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

Client &Server::getClient(int clientFD)
{
	std::map<int, Client*>::iterator it = _clients.find(clientFD);

	if (it == _clients.end())
		throw std::runtime_error("Client not found");

	return (*(it->second));
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

	if (it == _channels.end()) // change throw to some different return
		return (NULL);
		// throw std::runtime_error("Channel not found");

	return (&(it->second));
}

// JOIN
void Server::handleJoin(Parser& parser, int clientFd)
{
	const std::vector<std::string> &params = parser.getParams();
	std::vector<std::string>	channels;
	std::vector<std::string>	keys;

	// we can have 1 param or 2 params or 1 "0" param
	if (params.size() < 2 && params[0] == "0") // check if after "0" we have nothing
	{
		// leave all channels
	}
	else if (params.size() == 2)
	{
		channels = splitParams(params[0], ',');
		if (!params[1].empty())
			keys = splitParams(params[1], ',');

		for (int i; i < channels.size(); ++i)
		{
			if (i < keys.size()) // key (password) provided
			{
				// > join channel[i] with a given key & check for errors
				// std::map<std::string, Channel>	_channels;
				if (keys[i] == getChannel(channels[i])->getKey() && getChannel(channels[i])) // correct key && channel exists
				{
					getChannel(channels[i])->addMember(&getClient(clientFd)); // add client to the list in the channel
				}
				else if (!getChannel(channels[i])) // channel does not exist -> we create it
				{
					// CREATE CHANNEL
				}
				// channel.clients.add(this_client)
				continue; // ?
			}
			// > join keyless channels (or print error)
			// if (correct_key)
			//     channel.clients.add(this_client)
			if (getChannel(channels[i])->getKey()) // correct key && channel exists
			{
				getChannel(channels[i])->addMember(&getClient(clientFd)); // add client to the list in the channel
			}
			else if (!getChannel(channels[i])) // channel does not exist -> we create it
			{
				// CREATE CHANNEL
			}
		}





	}
	else
	{
		// incorrect input
	}

	// std::string channelParams = params[0];		// channels #foo,#bar
	// std::string keyParams = params[1];			// keys fubar,foobar


	// ==============
	// 1. params present?
	// 2. JOIN 0?
	// 3. split params[0] by ','
	// 4. if params[1] exists, split it by ','
	// 5. for every channel:
	//      validate channel name
	//      find or create channel
	//      get corresponding key if present
	//      check +k key
	//      check invite-only / limit / etc.
	//      add Client
	//      broadcast JOIN
	//      send topic
	//      send NAMES
}


// QUIT - we should firstly implement channels
// void Server::handleQuit(Parser& parser, int clientFd)
// {
// 	const std::string &quitMessage = parser.getTrailing();

// }
