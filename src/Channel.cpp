#include "../include/Channel.hpp"

Channel::Channel(const std::string &channelName):_channelName(channelName){}

void Channel::addMember(Client *c, std::string userMode){
	_members.insert(std::make_pair(c, userMode));
}

void Channel::rmvMember(Client *c){
	_members.erase(c);
}

const std::map<Client*, std::string> &Channel::getMembers() const{
	return _members;
}

const std::string& Channel::getChannelName() const {
    return _channelName;
}