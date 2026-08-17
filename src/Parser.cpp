#include "../include/Parser.hpp"

// Parser use grammar from RFC 2812 standard
// [ ":" prefix SPACE ] command [ params ] crlf
void Parser::parseGrammar(const std::string &msgIRC){

	_prefix.clear();
    _command.clear();
    _params.clear();
    _trailing.clear();

	std::string endSign = "\r\n";
	std::string msg = msgIRC;
	// According to RFC2812 empty messages are silently ignored
	if (msg.empty() || onlyWhitespace(msg))
		return;
	// parsing prefix
	if (msgIRC.at(0) == ':'){
		size_t end = msg.find(' ');
		if (end == std::string::npos)
			throw std::runtime_error("Invalid IRC message: malformed prefix");
		_prefix = msg.substr(1, end - 1);
		msg.erase(0, end + 1);
	}
	// parsing command
	size_t end = findTokenEnd(msg, endSign);
	if (end == 0)
		throw std::runtime_error("Invalid IRC message: missing command");
	// size_t end = msg.find(' ') || msg.find(endSign);
	_command = msg.substr(0, end);
	msg.erase(0, end);
	// Only 1 trailing param
	if (msg.compare(endSign) == 0)
		return ;
	if (!msg.empty() && msg[0] == ' ')
		msg.erase(0, 1);	
	// parsing middle params and trailing param
	while (msg.size() > 2){
		if (msg.at(0) == ':'){
			size_t end = msg.find(endSign);
			if (end == std::string::npos)
				throw std::runtime_error("Invalid IRC message: missing CRLF");
			_trailing = msg.substr(1, end - 1);
			return;
		}
		else{
			size_t end = findTokenEnd(msg, endSign);
			if (end == 0)
				throw std::runtime_error("Invalid IRC message: empty parameter");
			_params.push_back(msg.substr(0, end));
			// CRLF found -> no more parameters
			if (msg.compare(end, endSign.size(), endSign) == 0)
				return;
			msg.erase(0, end + 1);
		}
	}	
}
