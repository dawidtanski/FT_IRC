#include "../include/Channel.hpp"

Channel::Channel(const std::string &channelName):_channelName(channelName),_inviteOnly(false),_topicRestricted(false),_hasKey(false),_userLimit(0){}

bool Channel::memberIsOperator(const Client &user){

	for(std::map<Client*, std::string>::iterator it = _members.begin(); it != _members.end(); ++it){
		if (it->first == &user && it->second == "operator")
			return true;
	}
	return false;
}

bool Channel::isTopResMode(){
	if (_topicRestricted == true)
		return true;
	else
		return false;
}

//INVITE helper
bool Channel::isInviteOnlyMode()
{
	if (_inviteOnly == true)
		return (true);
	else
		return (false);
}

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

std::string	Channel::getKey(void)
{
	if (_hasKey)
		return (_key);
	return ("");
}

std::string	Channel::getChannelName(void)
{
	return (_channelName);
}
const std::string& Channel::getChannelName() const {
    return _channelName;
}

// INVITE helpers

void Channel::inviteUser(const std::string &nickname)
{

}

bool Channel::isInvited(const std::string &nickname) const
{

}

void Channel::removeInvite(const std::string &nickname)
{

}

void Channel::changeMemberMode(Client *c, std::string newMode){
    std::map<Client*, std::string>::iterator it = _members.find(c);
    if (it != _members.end())
        it->second = newMode;
}

void Channel::setInviteOnly(bool value){
	_inviteOnly = value;
}

void Channel::setTopicRestricted(bool value){
	_topicRestricted = value;
}

void Channel::setKey(const std::string &key){
	_key = key;
	_hasKey = !key.empty();
}

void Channel::setUserLimit(size_t limit){
	_userLimit = limit;
}

size_t Channel::getUserLimit() const{
	return _userLimit;
}
