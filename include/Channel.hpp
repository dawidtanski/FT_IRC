#pragma once

#include "irc.hpp"
#include "Client.hpp"

class Channel
{
	private:
		std::string _channelName;
		std::set<Client*> _members;

	public:

		Channel(const std::string &channelName);

		void addMember(Client *c);
		void rmvMember(Client *c);
		const std::set<Client*> &getMembers() const;
};