#pragma once
#include "irc.hpp"

int			sendall(int sockFD, const std::string &msg);
std::string	inet_ntop2(const sockaddr_storage& addr);
void		addToPollFDs(std::vector<struct pollfd>& pfds, int newFD);