#pragma once

#include "irc.hpp"
#include "Client.hpp"

class Channel
{
	private:
		std::string _channelName;
		std::map<Client*, std::string> _members;
		std::set<char> _mode;
		std::string _topic;

	public:

		Channel(const std::string &channelName);

		bool isMember(const Client &user);
		void addMember(Client *c, std::string userMode);
		void rmvMember(Client *c);
		const std::string getTopic() const;
		void setTopic(std:: string newTopic);
		const std::map<Client*,std::string> &getMembers() const;
		const std::string& getChannelName() const;
};