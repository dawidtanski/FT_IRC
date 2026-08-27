#pragma once

#include "irc.hpp"
#include "Client.hpp"

class Channel
{
	private:
		std::string			_channelName;
		std::set<Client*>	_members;
		std::set<char>		_mode;
		std::string			_key;
		bool				_hasKey;

	public:

		Channel(const std::string &channelName);

		void addMember(Client *c);
		void rmvMember(Client *c);
		const std::set<Client*> &getMembers() const;
		std::string	getKey(void);
		std::string	getChannelName(void);
};
