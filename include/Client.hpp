#pragma once
#include "irc.hpp"

class Client
{
	private:
		int			_fd;

		std::string _hostname; // ip
		std::string	_nickname; // max length 9 characters RFC 2812
		std::string	_username;
		std::string _realname;

		// MODES;
		bool _away;
		std::string _awayMessage;
		bool _invisible;
		bool _recvWallops;
		bool _restricted;
		bool _servNotices;
		bool _operator;

		bool		_auth;
		bool		_registered;

		std::string	_buffer;
		std::string _output;
		bool _outputFailed;
		bool _closing;
		bool _capNegotiating;
		Client(const Client &);
		Client &operator=(const Client &);

		std::set <std::string> _channelsList;


	public:
		Client(int fd, const std::string& ip);
		~Client();

		void joinChannel(const std::string &channelName);
		void quitChannel(const std::string &channelName);
		int sendMsg(const std::string &msg);
		bool flushOutput();
		bool hasOutput() const;
		bool outputFailed() const;
		void closeAfterOutput();
		bool isClosing() const;
		void setCapNegotiating(bool value);
		bool isCapNegotiating() const;

		// getters and setters
		bool				isAuth(void) const;
		void				setAuth(bool auth);
		bool				isRegistered(void) const;
		void				setRegistered(bool val);
		bool				isAway(void) const;
		void				setAway(bool val);
		const std::string &getAwayMessage() const;
		void setAwayMessage(const std::string &message);
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

		// Handling data
		void appendInput(const char *data, size_t size);
		bool popLine(std::string &line);
		size_t inputSize() const;



		const std::string	&getNickname() const;
		void				setNickname(const std::string &nickname);
		const std::string	&getUsername() const;
		void				setUsername(const std::string &username);
		int					getFD() const;
		const std::string	&getRealname() const;
		void				setRealname(const std::string &realname);
		const std::string	&getHostName() const;

		const std::set <std::string> &getChannels() const;
		const std::string	&getHostname() const;
};
