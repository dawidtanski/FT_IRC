#include "../include/Channel.hpp"

Channel::Channel(const std::string &channelName):_channelName(channelName){}

void Channel::addMember(Client *c){
	_members.insert(c);
}

void Channel::rmvMember(Client *c){
	_members.erase(c);
}

const std::set<Client*> &Channel::getMembers() const{
	return _members;
}

std::string	Channel::getKey(void)
{
	if (_hasKey)
		return (_key);
	return (NULL);
}

std::string	Channel::getChannelName(void)
{
	return (_channelName);
}
