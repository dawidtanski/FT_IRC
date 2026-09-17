#pragma once
#include "irc.hpp"

class Parser{

	private:
		std::string					_prefix;
		std::string					_command;
		std::vector<std::string>	_params;
		std::string					_trailing;
		bool							_hasTrailing;

	public:
		Parser() : _hasTrailing(false) {}
		const std::string				&getCommand() const;
		const std::vector<std::string>	&getParams() const;
		const std::string				&getTrailing() const;
		bool								 hasTrailing() const;
		const std::string				&getPrefix() const;

		// parsing
		void parseGrammar(const std::string &msgIRC);
};
