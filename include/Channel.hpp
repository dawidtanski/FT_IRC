#pragma once

#include "irc.hpp"
#include "Client.hpp"

class Channel
{
	private:
		std::string			_channelName;
		// std::set<Client*>	_members;
		std::set<char>		_mode; //TODO - maybe not necessary
		std::string			_key;
		std::map<Client*, std::string> _members;	//NOTE - Channel operator is smt different than server op.
		std::string _topic;
		bool _inviteOnly; //i
		bool _topicRestricted; //t
		bool _hasKey; //k
		size_t _userLimit;
		std::set<std::string>	_invitedUsers;

	public:

		Channel(const std::string &channelName);

		// void changeMemberMode(Client *c, std::string newMode);
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

		void inviteUser(const std::string &nickname);
		bool isInvited(const std::string &nickname) const;
		void removeInvite(const std::string &nickname);
};
