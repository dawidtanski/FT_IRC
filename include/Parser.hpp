#pragma once
#include "irc.hpp"

class Parser{

	private:
		std::string _prefix;
		std::string _command;
		std::vector<std::string> _params;
		std::string _trailing;


	public:
};