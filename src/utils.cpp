/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   utils.cpp                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: kjamrosz <kjamrosz@student.42warsaw.pl>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/17 12:10:40 by kjamrosz          #+#    #+#             */
/*   Updated: 2026/09/17 12:12:12 by kjamrosz         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../include/utils.hpp"

std::string inet_ntop2(const sockaddr_storage& addr)
{
	char buf[INET6_ADDRSTRLEN];

	if (addr.ss_family == AF_INET)
	{
		const sockaddr_in *sa4 = (const sockaddr_in *)&addr;
		if (!inet_ntop(sa4->sin_family, &sa4->sin_addr, buf, INET6_ADDRSTRLEN))
			throw std::runtime_error("inet_ntop failed");
		return buf;
	}
	else if (addr.ss_family == AF_INET6)
	{
		const sockaddr_in6 *sa6 = (const sockaddr_in6 *)&addr;
		if(!inet_ntop(sa6->sin6_family, &sa6->sin6_addr, buf, INET6_ADDRSTRLEN))
			throw std::runtime_error("inet_ntop failed");
		return buf;
	}
	else
		throw std::runtime_error("Couldn't convert address from binary to string");
}

void addToPollFDs(std::vector<struct pollfd>& pfds, int newFD)
{
	struct pollfd	pfd;

	pfd.fd = newFD;
	pfd.events = POLLIN;
	pfd.revents = 0;

	pfds.push_back(pfd);
}

void		rmvFromPollFDs(std::vector<struct pollfd>& pfds, int FD){

	std::vector<struct pollfd>::iterator it = pfds.begin();

	while (it != pfds.end()	){
		if (it->fd == FD){
			pfds.erase(it);
			return;
		}
		++it;
	}
}

bool onlyWhitespace(const std::string& s)
{
	for (size_t i = 0; i < s.size(); ++i)
		if (!isspace(static_cast<unsigned char>(s[i])))
			return false;
	return true;
}

size_t findTokenEnd(const std::string &msg, const std::string &endSign){

	size_t posSpace = msg.find(' ');
	size_t posTerminator = msg.find(endSign);

	if (posSpace == std::string::npos && posTerminator == std::string::npos)
		return msg.size();
	else if (posSpace == std::string::npos)
		return posTerminator;
	else if (posTerminator == std::string::npos)
		return posSpace;
	else
		return std::min(posSpace, posTerminator); 
}
// RFC1459 casemapping, also used for channel names.
std::string ircCaseFold(std::string value){
	for (size_t i = 0; i < value.size(); ++i){
		if (value[i] >= 'A' && value[i] <= 'Z')
			value[i] += 'a' - 'A';
		else if (value[i] == '[') value[i] = '{';
		else if (value[i] == ']') value[i] = '}';
		else if (value[i] == '\\') value[i] = '|';
		else if (value[i] == '^') value[i] = '~';
	}
	return value;
}
