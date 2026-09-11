#include "../include/Client.hpp"

Client::Client(int fd, const std::string& ip):_fd(fd), _hostname(ip), _auth(0), _away(false), _invicible(false), _recvWallops(false), _restricted(false), _servNotices(false) {
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

const std::string	&Client::getNickname() const
{
	return (_nickname);
}

void		Client::setNickname(const std::string &nickname)
{
	_nickname = nickname;
}

const std::string	&Client::getUsername() const
{
	return (_username);
}

void		Client::setUsername(const std::string &username)
{
	_username = username;
}

int			Client::getFD() const{
	return _fd;
}

const std::string &Client::getRealname() const
{
	return (_realname);
}

void Client::setRealname(const std::string &realname)
{
	_realname = realname;
}

const std::string&	Client::getHostName() const{
	return _hostname;
}

const std::set <std::string> &Client::getChannels() const{
	return _channelsList;
}

bool	Client::isAway(void) const{
	return _away;
}
void Client::setAway(bool val){
	if (val == true)
		_away = true;
	else
		_away = false;
}
bool	Client::isInvicible(void) const{
	return _invicible;
}
void Client::setInvicible(bool val){
	if (val == true)
		_invicible = true;
	else
		_invicible = false;
}

bool	Client::isRecvWallops(void) const{
	return _recvWallops;
}
void Client::setRecvWallops(bool val){
	if (val == true)
		_recvWallops = true;
	else
		_recvWallops = false;
}

bool	Client::isServNotices(void) const{
	return _servNotices;
}
void Client::setServNotices(bool val){
	if (val == true)
		_servNotices = true;
	else
		_servNotices = false;
}



// const std::string&	Client::getMode() const{
// 	return _userMode;
// }

