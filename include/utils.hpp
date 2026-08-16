#pragma once
#include "irc.hpp"

int			sendall(int sock_fd, char *buf, int *len);
std::string	inet_ntop2(const sockaddr_storage& addr);
void		addToPollFDs(std::vector<struct pollfd>& pfds, int newFD);