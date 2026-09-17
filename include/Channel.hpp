#pragma once

#include "irc.hpp"
#include "Client.hpp"

class Channel
{
	private:
		std::string			_channelName;
		std::string			_key;
		std::map<Client*, std::string> _members;
		std::string _topic;
		bool _inviteOnly;
		bool _topicRestricted;
		bool _hasKey;
		size_t _userLimit;
		std::set<int>	_invitedUsers;

	public:
		Channel(const std::string &channelName);

		bool memberIsOperator(const Client &user);
		bool isTopResMode();
		bool isInviteOnlyMode();
		void setInviteOnly(bool value);
		void setTopicRestricted(bool value);
		void setKey(const std::string &key);
		void setUserLimit(size_t limit);
		size_t getUserLimit() const;
		bool isMember(const Client &user);
		void addMember(Client *c, std::string userMode);
		void rmvMember(Client *c);
		void changeMemberMode(Client *c, std::string newMode);
		std::string	getKey(void);
		std::string	getChannelName(void);
		const std::string getTopic() const;
		void setTopic(std:: string newTopic);
		const std::map<Client*,std::string> &getMembers() const;
		const std::string& getChannelName() const;

		void inviteUser(int fd);
		bool isInvited(int fd) const;
		void removeInvite(int fd);
		bool isChannelEmpty(void);
};
