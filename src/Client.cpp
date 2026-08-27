#include "../include/Client.hpp"

Client::Client(int fd, const std::string& ip):_fd(fd), _hostname(ip), _auth(0){
}

Client::~Client(){
	if (_fd >= 0)
		close(_fd);
}

void Client::joinChannel(const std::string &channelName){
	_channelsList.insert(channelName);
}

void Client::quitChannel(const std::string &channelName){
	_channelsList.erase(channelName);
}

int Client::sendMsg(const std::string &msg){

	int n = sendall(_fd, msg);
	return n;
}

// getters and setters
bool		Client::isAuth(void) const
{
	return (_auth);
}

void		Client::setAuth(bool auth)
{
	_auth = auth;
}

std::string	Client::getNickname() const
{
	return (_nickname);
}

void		Client::setNickname(const std::string &nickname)
{
	_nickname = nickname;
}

std::string	Client::getUsername() const
{
	return (_username);
}

void		Client::setUsername(const std::string &username)
{
	_username = username;
}

const std::string Client::getRealname() const
{
	return (_realname);
}

void Client::setRealname(const std::string &realname)
{
	_realname = realname;
}

std::set <std::string> Client::getChannels(){
	return _channelsList;
}