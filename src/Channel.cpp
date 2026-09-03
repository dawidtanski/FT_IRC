#include "../include/Channel.hpp"

Channel::Channel(const std::string &channelName):_channelName(channelName){}

bool Channel::isMember(const Client &user)
{
    for (std::map<Client*, std::string>::const_iterator it = _members.begin();
         it != _members.end(); ++it)
    {
        if (it->first == &user)
            return true;
    }

    return false;
}

void Channel::addMember(Client *c, std::string userMode){
	_members.insert(std::make_pair(c, userMode));
}

void Channel::rmvMember(Client *c){
	_members.erase(c);
}

const std::string Channel::getTopic() const{
	return _topic;
}

void Channel::setTopic(std::string newTopic){
	_topic = newTopic;
}

const std::map<Client*, std::string> &Channel::getMembers() const{
	return _members;
}

const std::string& Channel::getChannelName() const {
    return _channelName;
}