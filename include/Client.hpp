#pragma once
#include "irc.hpp"


class Client
{
	private:
		int			_fd;

		std::string _hostname; // ip
		std::string	_nickname; // max length 9 characters RF2812
		std::string	_username;
		
		std::string _userMode; // user or operator
		std::string	_buffer;

		std::set <std::string> _channelsList;

		bool _auth;

	public:
		Client(int fd, const std::string& ip);
		~Client();

		void joinChannel(const std::string &channelName);
		void quitChannel(const std::string &channelName);
		int sendMsg(const std::string &msg);
};