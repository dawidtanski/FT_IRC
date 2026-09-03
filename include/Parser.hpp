#pragma once
#include "irc.hpp"

class Parser{

	private:
		std::string					_prefix;
		std::string					_command;
		std::vector<std::string>	_params;
		std::string					_trailing;
		bool							_hasTrailing;

		bool commandCheck(const std::string &cmd);

	public:
		// getters
		const std::string				&getCommand() const;
		const std::vector<std::string>	&getParams() const;
		const std::string				&getTrailing() const;
		bool								 hasTrailing() const;
		const std::string				&getPrefix() const;

		// parsing
		void parseGrammar(const std::string &msgIRC);
		
		// server logic functions moved to Server
};
