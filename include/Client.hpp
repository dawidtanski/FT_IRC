#pragma once
#include "irc.hpp"

class Client
{
	private:
		int			_fd;

		std::string _hostname; // ip
		std::string	_nickname; // max length 9 characters RF2812
		std::string	_username;
		std::string _realname; //ADDED

		// MODES;
		bool _away;
		bool _invisible;
		bool _recvWallops;
		bool _restricted;
		bool _servNotices;
		bool _operator;

		// std::string _userMode; // user or operator
		std::string	_buffer;

		std::set <std::string> _channelsList;

		bool		_auth;
		bool		_registered; //ADDED

	public:
		Client(int fd, const std::string& ip);
		~Client();

		void joinChannel(const std::string &channelName);
		void quitChannel(const std::string &channelName);
		int sendMsg(const std::string &msg);

		// getters and setters
		bool				isAuth(void) const;
		void				setAuth(bool auth);
		bool				isAway(void) const;
		void				setAway(bool val);
		bool				isInvisible() const;
		void				setInvisible(bool val);
		bool				isRecvWallops() const;
		void				setRecvWallops(bool val);
		bool				isServNotices() const;
		void				setServNotices(bool val);
		bool				isOperator() const;
		void				setOperator(bool val);
		bool				isRestricted() const;
		void				setRestricted(bool val);



		const std::string	&getNickname() const;
		void				setNickname(const std::string &nickname);
		const std::string	&getUsername() const;
		void				setUsername(const std::string &username);
		int					getFD() const;
		const std::string	&getRealname() const;
		void				setRealname(const std::string &realname);
		const std::string	&getHostName() const;
		// const std::string	&getMode() const;

		const std::set <std::string> &getChannels() const;

};
