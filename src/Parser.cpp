#include "../include/Parser.hpp"

// Parser use grammar from RFC 2812 standard
// [ ":" prefix SPACE ] command [ params ] crlf
void Parser::parseGrammar(const std::string &msgIRC){

	std::string msg = msgIRC;
	// According to RFC2812 empty messages are silently ignored
	if (msg.empty())
		return;
	if (msg.at(0) != ':')
		_prefix = "";
	else{
		// parsing prefix
		size_t start = msg.find(':');
		size_t end = msg.find(' ');
		_prefix = msg.substr(start + 1, end - (start + 1));
		msg.erase(0, end + 1);
	}
	// parsing command
	size_t end = msg.find(' ');
	_command = msg.substr(0, end + 1);
	msg.erase()

	// 
	
	// for(size_t i = 0; i <= msgIRC.size(); i++){
	// 	if (msgIRC.at(0) == ':')
	// 	{
	// 		++i;
	// 		while (msgIRC.at(i) != ' '){
				
	// 		}

	// 	}
	// }
}