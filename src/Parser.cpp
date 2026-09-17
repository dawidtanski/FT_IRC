/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Parser.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: kjamrosz <kjamrosz@student.42warsaw.pl>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/17 12:10:33 by kjamrosz          #+#    #+#             */
/*   Updated: 2026/09/17 12:12:12 by kjamrosz         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../include/Parser.hpp"

// getters
const std::string &Parser::getCommand() const
{
	return (_command);
}

const std::vector<std::string> &Parser::getParams() const
{
	return (_params);
}

const std::string &Parser::getTrailing() const
{
	return (_trailing);
}

bool Parser::hasTrailing() const
{
	return (_hasTrailing);
}

const std::string &Parser::getPrefix() const
{
	return (_prefix);
}

// Parse syntax independently of the supported command set.
void Parser::parseGrammar(const std::string &line){
	_prefix.clear();
	_command.clear();
	_params.clear();
	_trailing.clear();
	_hasTrailing = false;
	if (line.size() < 2 || line.size() > 512 || line.substr(line.size() - 2) != "\r\n")
		throw std::runtime_error("Invalid IRC line length or terminator");
	const std::string msg = line.substr(0, line.size() - 2);
	if (msg.find_first_of("\r\n") != std::string::npos || msg.find('\0') != std::string::npos)
		throw std::runtime_error("Invalid character in IRC message");
	size_t pos = msg.find_first_not_of(' ');
	if (pos == std::string::npos)
		return;
	if (msg[pos] == ':'){
		const size_t end = msg.find(' ', pos);
		if (end == std::string::npos || end == pos + 1)
			throw std::runtime_error("Invalid prefix");
		_prefix = msg.substr(pos + 1, end - pos - 1);
		pos = msg.find_first_not_of(' ', end);
	}
	if (pos == std::string::npos)
		throw std::runtime_error("Missing command");
	size_t end = msg.find(' ', pos);
	_command = msg.substr(pos, end == std::string::npos ? end : end - pos);
	for (size_t i = 0; i < _command.size(); ++i){
		if (_command[i] >= 'a' && _command[i] <= 'z')
			_command[i] -= 'a' - 'A';
		if (_command[i] < 'A' || _command[i] > 'Z')
			throw std::runtime_error("Invalid command");
	}
	pos = end;
	while (pos != std::string::npos){
		pos = msg.find_first_not_of(' ', pos);
		if (pos == std::string::npos)
			break;
		if (_params.size() == 15)
			throw std::runtime_error("Too many parameters");
		if (msg[pos] == ':'){
			_hasTrailing = true;
			_trailing = msg.substr(pos + 1);
			_params.push_back(_trailing);
			break;
		}
		end = msg.find(' ', pos);
		_params.push_back(msg.substr(pos, end == std::string::npos ? end : end - pos));
		pos = end;
	}
}
