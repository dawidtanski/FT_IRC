#include "../include/utils.hpp"

int sendall(int sockFD, const std::string &msg){

	size_t total = 0;
	size_t len = msg.size();

	while (total < len){
		int n = send(sockFD, msg.c_str() + total, len - total, 0);
		if (n <= 0)
			return -1;
		total += n;
	}
	return 0;
}

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

bool onlyWhitespace(const std::string& s)
{
	for (size_t i = 0; i < s.size(); ++i)
		if (!isspace(s[i]))
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