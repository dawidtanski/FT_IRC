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