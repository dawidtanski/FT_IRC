/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: kjamrosz <kjamrosz@student.42warsaw.pl>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/17 12:10:30 by kjamrosz          #+#    #+#             */
/*   Updated: 2026/09/17 12:12:12 by kjamrosz         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../include/Client.hpp"

Client::Client(int fd, const std::string& ip):_fd(fd), _hostname(ip), _away(false), _invisible(false), _recvWallops(false), _restricted(false), _servNotices(false), _operator(false), _auth(false), _registered(false), _outputFailed(false), _closing(false), _capNegotiating(false){
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

// Queue only: the event loop performs the actual send after POLLOUT.
int Client::sendMsg(const std::string &msg){
	if (_outputFailed)
		return -1;
	try {
		std::string line = msg;
		if (line.size() > 512)
			line = line.substr(0, 510) + "\r\n";
		if (_output.size() + line.size() > 262144){
			_outputFailed = true;
			return -1;
		}
		_output.append(line);
	}
	catch (const std::bad_alloc &) {
		_outputFailed = true;
		return -1;
	}
	return 0;
}

bool Client::flushOutput(){
	if (_output.empty())
		return true;
	const ssize_t sent = send(_fd, _output.data(), _output.size(), 0);
	if (sent > 0){
		_output.erase(0, static_cast<size_t>(sent));
		return true;
	}
	return sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR);
}

bool Client::hasOutput() const { return !_output.empty(); }
bool Client::outputFailed() const { return _outputFailed; }
void Client::closeAfterOutput() { _closing = true; }
bool Client::isClosing() const { return _closing; }
void Client::setCapNegotiating(bool value) { _capNegotiating = value; }
bool Client::isCapNegotiating() const { return _capNegotiating; }

// getters and setters
bool		Client::isAuth(void) const
{
	return (_auth);
}

void		Client::setAuth(bool auth)
{
	_auth = auth;
}

bool		Client::isRegistered(void) const{
	return _registered;
}
void		Client::setRegistered(bool val){
	_registered = val;
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
bool	Client::isInvisible(void) const{
	return _invisible;
}
void Client::setInvisible(bool val){
	if (val == true)
		_invisible = true;
	else
		_invisible = false;
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

bool	Client::isOperator(void) const{
	return _operator;
}
void Client::setOperator(bool val){
	if (val == true)
		_operator = true;
	else
		_operator = false;
}

bool	Client::isRestricted(void) const{
	return _restricted;
}
void Client::setRestricted(bool val){
	if (val == true)
		_restricted = true;
	else
		_restricted = false;
}

const std::string &Client::getAwayMessage() const{
	return _awayMessage;
}

void Client::setAwayMessage(const std::string &message){
	_awayMessage = message;
}
const std::string &Client::getHostname() const
{
	return (_hostname);
}

void Client::appendInput(const char *data, size_t size){
	_buffer.append(data, size);
}

bool Client::popLine(std:: string &line){
	const size_t end = _buffer.find(ENDSIGN);

	if (end == std::string::npos)
		return false;
	line = _buffer.substr(0, end + 2);
	_buffer.erase(0, end + 2);

	return true;
}

size_t Client::inputSize() const
{
    return _buffer.size();
}
