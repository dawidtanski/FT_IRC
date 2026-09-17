#pragma once
#include "irc.hpp"

// network
std::string ircCaseFold(std::string value);
std::string	inet_ntop2(const sockaddr_storage& addr);
void		addToPollFDs(std::vector<struct pollfd>& pfds, int newFD);
void		rmvFromPollFDs(std::vector<struct pollfd>& pfds, int newFD);

// strings
bool		onlyWhitespace(const std::string& s);
size_t 		findTokenEnd(const std::string &msg, const std::string &endSign);
