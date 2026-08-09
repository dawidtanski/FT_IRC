#include "irc.hpp"


class Client
{
	public:
		int			fd;
		std::string	nickname;
		std::string	username;
		std::string	buffer;
};